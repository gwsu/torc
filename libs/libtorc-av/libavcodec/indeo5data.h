/*
 * Indeo Video Interactive 5 compatible decoder
 * Copyright (c) 2009 Maxim Poliakovski
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * This file contains data needed for the Indeo5 decoder.
 */

#ifndef AVCODEC_INDEO5DATA_H
#define AVCODEC_INDEO5DATA_H

#include <stdint.h>

/**
 *  standard picture dimensions (width, height divided by 4)
 */
static const uint8_t ivi5_common_pic_sizes[30] = {
    160, 120, 80, 60, 40, 30, 176, 120, 88, 60, 88, 72, 44, 36, 60, 45, 160, 60,
    176,  60, 20, 15, 22, 18,   0,   0,  0,  0,  0,  0
};


/**
 *  Indeo5 dequantization matrixes consist of two tables: base table
 *  and scale table. The base table defines the dequantization matrix
 *  itself and the scale table tells how this matrix should be scaled
 *  for a particular quant level (0...24).
 *
 *  ivi5_base_quant_bbb_ttt  - base  tables for block size 'bbb' of type 'ttt'
 *  ivi5_scale_quant_bbb_ttt - scale tables for block size 'bbb' of type 'ttt'
 */
static const uint16_t ivi5_base_quant_8x8_inter[5][64] = {
    {0x26, 0x3a, 0x3e, 0x46, 0x4a, 0x4e, 0x52, 0x5a, 0x3a, 0x3e, 0x42, 0x46, 0x4a, 0x4e, 0x56, 0x5e,
     0x3e, 0x42, 0x46, 0x48, 0x4c, 0x52, 0x5a, 0x62, 0x46, 0x46, 0x48, 0x4a, 0x4e, 0x56, 0x5e, 0x66,
     0x4a, 0x4a, 0x4c, 0x4e, 0x52, 0x5a, 0x62, 0x6a, 0x4e, 0x4e, 0x52, 0x56, 0x5a, 0x5e, 0x66, 0x6e,
     0x52, 0x56, 0x5a, 0x5e, 0x62, 0x66, 0x6a, 0x72, 0x5a, 0x5e, 0x62, 0x66, 0x6a, 0x6e, 0x72, 0x76,
    },
    {0x26, 0x3a, 0x3e, 0x46, 0x4a, 0x4e, 0x52, 0x5a, 0x3a, 0x3e, 0x42, 0x46, 0x4a, 0x4e, 0x56, 0x5e,
     0x3e, 0x42, 0x46, 0x48, 0x4c, 0x52, 0x5a, 0x62, 0x46, 0x46, 0x48, 0x4a, 0x4e, 0x56, 0x5e, 0x66,
     0x4a, 0x4a, 0x4c, 0x4e, 0x52, 0x5a, 0x62, 0x6a, 0x4e, 0x4e, 0x52, 0x56, 0x5a, 0x5e, 0x66, 0x6e,
     0x52, 0x56, 0x5a, 0x5e, 0x62, 0x66, 0x6a, 0x72, 0x5a, 0x5e, 0x62, 0x66, 0x6a, 0x6e, 0x72, 0x76,
    },
    {0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2, 0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2,
     0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2, 0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2,
     0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2, 0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2,
     0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2, 0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2,
    },
    {0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
     0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xd4, 0xd4, 0xd4, 0xd4, 0xd4, 0xd4, 0xd4, 0xd4,
     0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2,
     0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2,
    },
    {0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e,
     0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e,
     0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e,
     0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e,
    }
};

static const uint16_t ivi5_base_quant_8x8_intra[5][64] = {
    {0x1a, 0x2e, 0x36, 0x42, 0x46, 0x4a, 0x4e, 0x5a, 0x2e, 0x32, 0x3e, 0x42, 0x46, 0x4e, 0x56, 0x6a,
     0x36, 0x3e, 0x3e, 0x44, 0x4a, 0x54, 0x66, 0x72, 0x42, 0x42, 0x44, 0x4a, 0x52, 0x62, 0x6c, 0x7a,
     0x46, 0x46, 0x4a, 0x52, 0x5e, 0x66, 0x72, 0x8e, 0x4a, 0x4e, 0x54, 0x62, 0x66, 0x6e, 0x86, 0xa6,
     0x4e, 0x56, 0x66, 0x6c, 0x72, 0x86, 0x9a, 0xca, 0x5a, 0x6a, 0x72, 0x7a, 0x8e, 0xa6, 0xca, 0xfe,
    },
    {0x26, 0x3a, 0x3e, 0x46, 0x4a, 0x4e, 0x52, 0x5a, 0x3a, 0x3e, 0x42, 0x46, 0x4a, 0x4e, 0x56, 0x5e,
     0x3e, 0x42, 0x46, 0x48, 0x4c, 0x52, 0x5a, 0x62, 0x46, 0x46, 0x48, 0x4a, 0x4e, 0x56, 0x5e, 0x66,
     0x4a, 0x4a, 0x4c, 0x4e, 0x52, 0x5a, 0x62, 0x6a, 0x4e, 0x4e, 0x52, 0x56, 0x5a, 0x5e, 0x66, 0x6e,
     0x52, 0x56, 0x5a, 0x5e, 0x62, 0x66, 0x6a, 0x72, 0x5a, 0x5e, 0x62, 0x66, 0x6a, 0x6e, 0x72, 0x76,
    },
    {0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2, 0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2,
     0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2, 0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2,
     0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2, 0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2,
     0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2, 0x4e, 0xaa, 0xf2, 0xd4, 0xde, 0xc2, 0xd6, 0xc2,
    },
    {0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0x4e, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
     0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xd4, 0xd4, 0xd4, 0xd4, 0xd4, 0xd4, 0xd4, 0xd4,
     0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xde, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2,
     0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2, 0xc2,
    },
    {0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e,
     0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e,
     0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e,
     0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e, 0x5e,
    }
};

static const uint16_t ivi5_base_quant_4x4_inter[16] = {
    0x1e, 0x3e, 0x4a, 0x52, 0x3e, 0x4a, 0x52, 0x56, 0x4a, 0x52, 0x56, 0x5e, 0x52, 0x56, 0x5e, 0x66
};

static const uint16_t ivi5_base_quant_4x4_intra[16] = {
    0x1e, 0x3e, 0x4a, 0x52, 0x3e, 0x4a, 0x52, 0x5e, 0x4a, 0x52, 0x5e, 0x7a, 0x52, 0x5e, 0x7a, 0x92
};


static const uint8_t ivi5_scale_quant_8x8_inter[5][24] = {
    {0x0b, 0x11, 0x13, 0x14, 0x15, 0x16, 0x18, 0x1a, 0x1b, 0x1d, 0x20, 0x22,
     0x23, 0x25, 0x28, 0x2a, 0x2e, 0x32, 0x35, 0x39, 0x3d, 0x41, 0x44, 0x4a,
    },
    {0x07, 0x14, 0x16, 0x18, 0x1b, 0x1e, 0x22, 0x25, 0x29, 0x2d, 0x31, 0x35,
     0x3a, 0x3f, 0x44, 0x4a, 0x50, 0x56, 0x5c, 0x63, 0x6a, 0x71, 0x78, 0x7e,
    },
    {0x15, 0x25, 0x28, 0x2d, 0x30, 0x34, 0x3a, 0x3d, 0x42, 0x48, 0x4c, 0x51,
     0x56, 0x5b, 0x60, 0x65, 0x6b, 0x70, 0x76, 0x7c, 0x82, 0x88, 0x8f, 0x97,
    },
    {0x13, 0x1f, 0x20, 0x22, 0x25, 0x28, 0x2b, 0x2d, 0x30, 0x33, 0x36, 0x39,
     0x3c, 0x3f, 0x42, 0x45, 0x48, 0x4b, 0x4e, 0x52, 0x56, 0x5a, 0x5e, 0x62,
    },
    {0x3c, 0x52, 0x58, 0x5d, 0x63, 0x68, 0x68, 0x6d, 0x73, 0x78, 0x7c, 0x80,
     0x84, 0x89, 0x8e, 0x93, 0x98, 0x9d, 0xa3, 0xa9, 0xad, 0xb1, 0xb5, 0xba,
    },
};

static const uint8_t ivi5_scale_quant_8x8_intra[5][24] = {
    {0x0b, 0x0e, 0x10, 0x12, 0x14, 0x16, 0x17, 0x18, 0x1a, 0x1c, 0x1e, 0x20,
     0x22, 0x24, 0x27, 0x28, 0x2a, 0x2d, 0x2f, 0x31, 0x34, 0x37, 0x39, 0x3c,
    },
    {0x01, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1b, 0x1e, 0x22, 0x25, 0x28, 0x2c,
     0x30, 0x34, 0x38, 0x3d, 0x42, 0x47, 0x4c, 0x52, 0x58, 0x5e, 0x65, 0x6c,
    },
    {0x13, 0x22, 0x27, 0x2a, 0x2d, 0x33, 0x36, 0x3c, 0x41, 0x45, 0x49, 0x4e,
     0x53, 0x58, 0x5d, 0x63, 0x69, 0x6f, 0x75, 0x7c, 0x82, 0x88, 0x8e, 0x95,
    },
    {0x13, 0x1f, 0x21, 0x24, 0x27, 0x29, 0x2d, 0x2f, 0x34, 0x37, 0x3a, 0x3d,
     0x40, 0x44, 0x48, 0x4c, 0x4f, 0x52, 0x56, 0x5a, 0x5e, 0x62, 0x66, 0x6b,
    },
    {0x31, 0x42, 0x47, 0x47, 0x4d, 0x52, 0x58, 0x58, 0x5d, 0x63, 0x67, 0x6b,
     0x6f, 0x73, 0x78, 0x7c, 0x80, 0x84, 0x89, 0x8e, 0x93, 0x98, 0x9d, 0xa4,
    }
};

static const uint8_t ivi5_scale_quant_4x4_inter[24] = {
    0x0b, 0x0d, 0x0d, 0x0e, 0x11, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
};

static const uint8_t ivi5_scale_quant_4x4_intra[24] = {
    0x01, 0x0b, 0x0b, 0x0d, 0x0d, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x13, 0x14,
    0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20
};


#endif /* AVCODEC_INDEO5DATA_H */
