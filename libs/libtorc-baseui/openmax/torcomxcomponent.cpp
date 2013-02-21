/* Class TorcOMXComponent
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QElapsedTimer>

// Torc
#include "torclogging.h"
#include "torcomxport.h"
#include "torcomxcomponent.h"

TorcOMXEvent::TorcOMXEvent(OMX_EVENTTYPE Type, OMX_U32 Data1, OMX_U32 Data2)
  : m_type(Type),
    m_data1(Data1),
    m_data2(Data2)
{
}

static OMX_CALLBACKTYPE gCallbacks;

TorcOMXComponent::TorcOMXComponent(TorcOMXCore *Core, OMX_STRING Component, OMX_INDEXTYPE Index)
  : m_valid(false),
    m_core(Core),
    m_handle(NULL),
    m_lock(new QMutex(QMutex::Recursive)),
    m_componentName(Component),
    m_indexType(Index),
    m_bufferedInput(-1),
    m_bufferedOutput(-1),
    m_eventQueueLock(new QMutex())
{
    gCallbacks.EventHandler    = &EventHandlerCallback;
    gCallbacks.EmptyBufferDone = &EmptyBufferDoneCallback;
    gCallbacks.FillBufferDone  = &FillBufferDoneCallback;

    if (!m_core)
        return;

    OMX_ERRORTYPE status = m_core->m_omxGetHandle(&m_handle, Component, this, &gCallbacks);
    if (status != OMX_ErrorNone || !m_handle)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to get handle").arg(m_componentName));
        return;
    }

    OMX_PORT_PARAM_TYPE portparameters;
    OMX_INITSTRUCTURE(portparameters);

    if (OMX_GetParameter(m_handle, Index, &portparameters) != OMX_ErrorNone)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to get port parameters").arg(m_componentName));
        return;
    }

    m_valid = true;
    if (DisablePorts(Index) != OMX_ErrorNone)
    {
        m_valid = false;
        return;
    }

    QString inports;
    QString outports;

    for (OMX_U32 port = portparameters.nStartPortNumber; port < (portparameters.nStartPortNumber + portparameters.nPorts); ++port)
    {
        OMX_PARAM_PORTDEFINITIONTYPE portdefinition;
        OMX_INITSTRUCTURE(portdefinition);
        portdefinition.nPortIndex = port;

        OMX_ERRORTYPE error = OMX_GetParameter(m_handle, OMX_IndexParamPortDefinition, &portdefinition);
        if (OMX_ErrorNone == error)
        {
            if (OMX_DirInput == portdefinition.eDir)
            {
                inports += QString("%1 ").arg(port);
                m_inputPorts.append(new TorcOMXPort(this, m_handle, port));
            }
            else if (OMX_DirOutput == portdefinition.eDir)
            {
                outports += QString("%1 ").arg(port);
                m_outputPorts.append(new TorcOMXPort(this, m_handle, port));
            }
        }
        else
        {
            OMX_ERROR(error, m_componentName, "Failed to get port definition");
        }
    }

    LOG(VB_GENERAL, LOG_INFO, QString("%1: %2 input ports (%3), %4 output ports (%5)")
        .arg(m_componentName).arg(m_inputPorts.size()).arg(inports).arg(m_outputPorts.size()).arg(outports));
}

TorcOMXComponent::~TorcOMXComponent()
{
    {
        QMutexLocker locker(m_lock);

        while (!m_inputPorts.isEmpty())
            delete m_inputPorts.takeLast();
        while (!m_outputPorts.isEmpty())
            delete m_outputPorts.takeLast();

        if (m_core && m_handle)
            m_core->m_omxFreeHandle(m_handle);
        m_handle       = NULL;
    }

    delete m_lock;
    delete m_eventQueueLock;
}

bool TorcOMXComponent::IsValid(void)
{
    return m_valid;
}

OMX_ERRORTYPE TorcOMXComponent::EventHandlerCallback(OMX_HANDLETYPE Component, OMX_PTR OMXComponent, OMX_EVENTTYPE Event, OMX_U32 Data1, OMX_U32 Data2, OMX_PTR EventData)
{
    TorcOMXComponent *component = static_cast<TorcOMXComponent*>(OMXComponent);
    if (component)
        return component->EventHandler(Component, Event, Data1, Data2, EventData);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::EmptyBufferDoneCallback(OMX_HANDLETYPE Component, OMX_PTR OMXComponent, OMX_BUFFERHEADERTYPE *Buffer)
{
    TorcOMXComponent *component = static_cast<TorcOMXComponent*>(OMXComponent);
    if (component)
        return component->EmptyBufferDone(Component, Buffer);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::FillBufferDoneCallback(OMX_HANDLETYPE Component, OMX_PTR OMXComponent, OMX_BUFFERHEADERTYPE *Buffer)
{
    TorcOMXComponent *component = static_cast<TorcOMXComponent*>(OMXComponent);
    if (component)
        return component->FillBufferDone(Component, Buffer);

    return OMX_ErrorNone;
}

QString TorcOMXComponent::GetName(void)
{
    return m_componentName;
}

OMX_HANDLETYPE TorcOMXComponent::GetHandle(void)
{
    return m_handle;
}

OMX_ERRORTYPE TorcOMXComponent::SetState(OMX_STATETYPE State)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);
    OMX_ERRORTYPE error = OMX_SendCommand(m_handle, OMX_CommandStateSet, State, NULL);

    if (OMX_ErrorSameState == error)
    {
        return OMX_ErrorNone;
    }
    else if (OMX_ErrorNone == error)
    {
        error = WaitForResponse(OMX_CommandStateSet, State, 1000);
        if (OMX_ErrorSameState == error || OMX_ErrorNone == error)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("%1: Set state to %2").arg(m_componentName).arg(StateToString(State)));
            return OMX_ErrorNone;
        }
    }

    OMX_ERROR(error, m_componentName, "Failed to set state");
    return error;
}

OMX_STATETYPE TorcOMXComponent::GetState(void)
{
    if (!m_valid)
        return OMX_StateInvalid;

    QMutexLocker locker(m_lock);

    OMX_STATETYPE state;
    OMX_ERRORTYPE error = OMX_GetState(m_handle, &state);
    if (OMX_ErrorNone == error)
        return state;

    OMX_ERROR(error, m_componentName, "Failed to get state");
    return OMX_StateInvalid;
}

OMX_ERRORTYPE TorcOMXComponent::SetParameter(OMX_INDEXTYPE Index, OMX_PTR Structure)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_CHECK(OMX_SetParameter(m_handle, Index, Structure), m_componentName, "Failed to set parameter");
    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::GetParameter(OMX_INDEXTYPE Index, OMX_PTR Structure)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_CHECK(OMX_GetParameter(m_handle, Index, Structure), m_componentName, "Failed to get parameter");
    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::SetConfig(OMX_INDEXTYPE Index, OMX_PTR Structure)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_CHECK(OMX_SetConfig(m_handle, Index, Structure), m_componentName, "Failed to set config");
    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::GetConfig(OMX_INDEXTYPE Index, OMX_PTR Structure)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_CHECK(OMX_GetConfig(m_handle, Index, Structure), m_componentName, "Failed to get config");
    return OMX_ErrorNone;
}

OMX_U32 TorcOMXComponent::GetInputPort(OMX_U32 Index)
{
    QMutexLocker locker(m_lock);

    if (Index < (OMX_U32)m_inputPorts.size())
        return m_inputPorts.at(Index)->GetPort();
    return 0;
}

OMX_U32 TorcOMXComponent::GetOutputPort(OMX_U32 Index)
{
    QMutexLocker locker(m_lock);

    if (Index < (OMX_U32)m_outputPorts.size())
        return m_outputPorts.at(Index)->GetPort();
    return 0;
}

OMX_ERRORTYPE TorcOMXComponent::EnablePort(OMX_DIRTYPE InOut, OMX_U32 Index, bool Enable)
{
    QMutexLocker locker(m_lock);

    if (OMX_DirInput == InOut && (Index < (OMX_U32)m_inputPorts.size()))
    {
        return m_inputPorts.at(Index)->EnablePort(Enable);
    }
    else if (OMX_DirOutput == InOut && (Index < (OMX_U32)m_outputPorts.size()))
    {
        return m_outputPorts.at(Index)->EnablePort(Enable);
    }

    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE TorcOMXComponent::EmptyThisBuffer(OMX_BUFFERHEADERTYPE *Buffer)
{
    if (!m_valid || !Buffer)
        return OMX_ErrorUndefined;

    OMX_CHECK(OMX_EmptyThisBuffer(m_handle, Buffer), m_componentName, "EmptyThisBuffer failed");
    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::FillThisBuffer(OMX_BUFFERHEADERTYPE *Buffer)
{
    if (!m_valid || !Buffer)
        return OMX_ErrorUndefined;

    OMX_CHECK(OMX_FillThisBuffer(m_handle, Buffer), m_componentName, "FillThisBuffer failed");
    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::CreateBuffers(OMX_DIRTYPE InOut, OMX_U32 Index)
{
    OMX_STATETYPE state = GetState();
    if(state != OMX_StateIdle)
    {
        if(state != OMX_StateLoaded)
            SetState(OMX_StateLoaded);
        SetState(OMX_StateIdle);
    }

    if (OMX_DirInput == InOut && (Index < (OMX_U32)m_inputPorts.size()))
    {
        if (m_bufferedInput > -1)
            LOG(VB_GENERAL, LOG_WARNING, "Allocating buffers for more than one input");
        m_bufferedInput = (OMX_S32)Index;
        return m_inputPorts.at(Index)->CreateBuffers();
    }
    else if (OMX_DirOutput == InOut && (Index < (OMX_U32)m_outputPorts.size()))
    {
        if (m_bufferedOutput > -1)
            LOG(VB_GENERAL, LOG_WARNING, "Allocating buffers for more than one input");
        m_bufferedOutput = (OMX_S32)Index;
        return m_outputPorts.at(Index)->CreateBuffers();
    }

    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE TorcOMXComponent::DestroyBuffers(OMX_DIRTYPE InOut, OMX_U32 Index)
{
    OMX_STATETYPE state = GetState();
    if(state != OMX_StateIdle)
    {
        if(state != OMX_StateLoaded)
            SetState(OMX_StateLoaded);
        SetState(OMX_StateIdle);
    }

    if (OMX_DirInput == InOut && (Index < (OMX_U32)m_inputPorts.size()))
        return m_inputPorts.at(Index)->DestroyBuffers();
    else if (OMX_DirOutput == InOut && (Index < (OMX_U32)m_outputPorts.size()))
        return m_outputPorts.at(Index)->DestroyBuffers();

    return OMX_ErrorUndefined;
}

OMX_BUFFERHEADERTYPE* TorcOMXComponent::GetInputBuffer(OMX_U32 Index, OMX_U32 Timeout)
{
    // TODO not locked
    if (Index < (OMX_U32)m_inputPorts.size())
        return m_inputPorts.at(Index)->GetBuffer(Timeout);
    return NULL;
}

OMX_ERRORTYPE TorcOMXComponent::FlushBuffer(OMX_DIRTYPE InOut, OMX_U32 Index)
{
    QMutexLocker locker(m_lock);

    if (OMX_DirInput == InOut && (Index < (OMX_U32)m_inputPorts.size()))
    {
        OMX_CHECK(m_inputPorts.at(Index)->Flush(), m_componentName, "Failed to flush input buffers");
    }
    else if (OMX_DirOutput == InOut && (Index < (OMX_U32)m_outputPorts.size()))
    {
        OMX_CHECK(m_outputPorts.at(Index)->Flush(), m_componentName, "Failed to flush output buffers");
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::EventHandler(OMX_HANDLETYPE Component, OMX_EVENTTYPE Event, OMX_U32 Data1, OMX_U32 Data2, OMX_PTR EventData)
{
    if (m_handle != Component)
        return OMX_ErrorBadParameter;

    m_eventQueueLock->lock();
    m_eventQueue.append(TorcOMXEvent(Event, Data1, Data2));
    m_eventQueueLock->unlock();
    LOG(VB_GENERAL, LOG_DEBUG, QString("Event: %1 %2 %3").arg(EventToString(Event)).arg(Data1).arg(Data2));

    m_eventQueueWait.wakeAll();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::EmptyBufferDone(OMX_HANDLETYPE Component, OMX_BUFFERHEADERTYPE *Buffer)
{
    if (m_handle != Component)
        return OMX_ErrorBadParameter;

    if (m_bufferedInput > -1)
        return m_inputPorts.at(m_bufferedInput)->MakeAvailable(Buffer);

    LOG(VB_GENERAL, LOG_ERR, "No buffers allocated for input");
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE TorcOMXComponent::FillBufferDone(OMX_HANDLETYPE Component, OMX_BUFFERHEADERTYPE *Buffer)
{
    if (m_handle != Component)
        return OMX_ErrorBadParameter;

    if (m_bufferedOutput > -1)
        return m_outputPorts.at(m_bufferedOutput)->MakeAvailable(Buffer);

    LOG(VB_GENERAL, LOG_ERR, "No buffers allocated for output");
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE TorcOMXComponent::SendCommand(OMX_COMMANDTYPE Command, OMX_U32 Parameter, OMX_PTR Data)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_CHECK(OMX_SendCommand(m_handle, Command, Parameter, Data), m_componentName, "Failed to send command");
    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::WaitForResponse(OMX_U32 Command, OMX_U32 Data2, OMX_S32 Timeout)
{
    QElapsedTimer timer;
    timer.start();

    LOG(VB_GENERAL, LOG_DEBUG, QString("%1: Waiting for %2 %3")
        .arg(m_componentName).arg(CommandToString((OMX_COMMANDTYPE)Command)).arg(Data2));

    while (timer.elapsed() < Timeout)
    {
        m_eventQueueLock->lock();

        QList<TorcOMXEvent>::iterator it = m_eventQueue.begin();

        for ( ; it != m_eventQueue.end(); ++it)
        {
            if ((*it).m_type == OMX_EventCmdComplete && (*it).m_data1 == Command && (*it).m_data2 == Data2)
            {
                m_eventQueue.erase(it);
                m_eventQueueLock->unlock();
                return OMX_ErrorNone;
            }
            else if ((*it).m_type == OMX_EventError)
            {
                if ((*it).m_data1 == (OMX_U32)OMX_ErrorSameState && (*it).m_data2 == 1)
                {
                    m_eventQueue.erase(it);
                    m_eventQueueLock->unlock();
                    return OMX_ErrorNone;
                }

                LOG(VB_GENERAL, LOG_ERR, QString("%1: Error event '%2' data2 %3")
                    .arg(m_componentName).arg(ErrorToString((OMX_ERRORTYPE)(*it).m_data1)).arg((*it).m_data2));
                OMX_ERRORTYPE error = (OMX_ERRORTYPE)(*it).m_data1;
                m_eventQueue.erase(it);
                m_eventQueueLock->unlock();
                return error;
            }
        }

        m_eventQueueWait.wait(m_eventQueueLock, 50);
        m_eventQueueLock->unlock();
    }

    LOG(VB_GENERAL, LOG_INFO, QString("%1: Response never received for command %2")
        .arg(m_componentName).arg(CommandToString((OMX_COMMANDTYPE)Command)));
    return OMX_ErrorMax;
}

OMX_ERRORTYPE TorcOMXComponent::DisablePorts(OMX_INDEXTYPE Index)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_PORT_PARAM_TYPE portparameters;
    OMX_INITSTRUCTURE(portparameters);

    OMX_CHECK(OMX_GetParameter(m_handle, Index, &portparameters), m_componentName, "Failed to get port parameters");

    for (OMX_U32 i = 0; i < portparameters.nPorts; ++i)
    {
        OMX_PARAM_PORTDEFINITIONTYPE portdefinition;
        OMX_INITSTRUCTURE(portdefinition);
        OMX_U32 portnumber = portparameters.nStartPortNumber + i;
        portdefinition.nPortIndex = portnumber;

        if (OMX_GetParameter(m_handle, OMX_IndexParamPortDefinition, &portdefinition) == OMX_ErrorNone)
        {
            OMX_ERRORTYPE error = OMX_SendCommand(m_handle, OMX_CommandPortDisable, portnumber, NULL);
            if (OMX_ErrorNone == error)
            {
                error = WaitForResponse(OMX_CommandPortDisable, portnumber, 100);
                if (OMX_ErrorNone != error)
                {
                    OMX_ERROR(error, m_componentName, "Failed to disable port");
                }
            }
            else
            {
                OMX_ERROR(error, m_componentName, "Failed to send command");
            }
        }
        else
        {
            portdefinition.bEnabled = OMX_FALSE;
        }
    }

    return OMX_ErrorNone;
}



