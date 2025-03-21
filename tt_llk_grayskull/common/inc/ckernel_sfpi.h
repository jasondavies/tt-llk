// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include "ckernel_template.h"
#include "ckernel.h"
#include "cmath_common.h"
#include "sfpi.h"

#pragma once

// Inlining the tests may make the tests that use a parameter fail to test the
// non-imm path as compiling w/ -flto will fill in the value as an immediate
#if defined(__GNUC__) && !defined(__clang__)
#define sfpi_test_noinline __attribute__ ((noinline))
#else
#define sfpi_test_noinline
#endif

using namespace ckernel;
using namespace sfpi;

namespace sfpi_test
{

sfpi_inline void copy_result_to_dreg0(int addr)
{
    dst_reg[0] = dst_reg[addr];
}

// Test infrastructure is set up to test float values, not ints
// Viewing the ints as floats leads to a mess (eg, denorms)
// Instead, compare in the kernel to the expected result and write a sentinal
// value for "pass" and the vInt v value for "fail"
// Assumes this code is called in an "inner" if
sfpi_inline void set_expected_result(int addr, float sentinel, int expected, vInt v)
{
    // Poor man's equals
    // Careful, the register is 19 bits and the immediate is sign extended 12
    // bits so comparing bit patterns w/ the MSB set won't work
    v_if (v >= expected && v < expected + 1) {
        dst_reg[addr] = sentinel;
    } v_else {
        dst_reg[addr] = v;
    }
    v_endif;
}

sfpi_inline vInt test_interleaved_scalar_vector_cond(bool scalar_bool, vFloat vec, float a, float b)
{
    if (scalar_bool) {
        return vec == a;
    } else {
        return vec == b;
    }
}

sfpi_test_noinline void test1()
{
    // Test SFPLOADI, SFPSTORE
    dst_reg[0] = 1.3f;
}

sfpi_test_noinline void test2()
{
#ifdef TEST_ASM
    // Test SFPMOV
    TTI_SFPLOAD(p_sfpu::LREG3, 0, 0); // load from dest into lreg3
    TTI_SFPMOV(0, p_sfpu::LREG3, p_sfpu::LREG3, 1);
    TTI_NOP; TTI_NOP;
    TTI_SFPSTORE(3, 0, 0);
#elif defined(TEST_BUILTINS)
    __rvtt_vec_t v1 = __builtin_rvtt_sfpload(0, 0);
    __rvtt_vec_t v2 = __builtin_rvtt_sfpmov(v1, 1);
    __builtin_rvtt_sfpnop();
    __builtin_rvtt_sfpnop();
    __builtin_rvtt_sfpstore_v(v2, 0, 0);
#else
    // Test SFPLOAD, SFPMOV
    dst_reg[2] = -dst_reg[0];

    // Out: ramp down from 0 to -63
    copy_result_to_dreg0(2);
#endif
}

sfpi_test_noinline void test3()
{
    // Test SFPENCC, SFPSETCC, SFPCOMPC, LOADI
    // Also, load after store (NOP)
    v_if(dst_reg[0] == 0.0F) {
        dst_reg[3] = 10.0F;
    } v_else {
        dst_reg[3] = 20.0F;
    }
    v_endif;

    v_if(dst_reg[0] == 2.0F) {
        vFloat a = 30.0F;
        dst_reg[3] = a;
    }
    v_endif;

    v_if(dst_reg[0] == 3.0F) {
        vInt a = 0x3F80;
        dst_reg[3] = a;
    }
    v_endif;

    v_if(dst_reg[0] == 4.0F) {
        vUInt a = 0x3F80;
        dst_reg[3] = a;
    }
    v_endif;

    v_if(dst_reg[0] == 5.0F) {
        vUInt a = 0xFFFF;
        dst_reg[3] = a;
    }
    v_endif;

    v_if(dst_reg[0] == 6.0F) {
        vFloat a = 120.0F;
        dst_reg[3] = a;
    }
    v_endif;

    v_if(dst_reg[0] == 7.0F) {
        dst_reg[3] = 0xFFFF;
    }
    v_endif;

    v_if(dst_reg[0] == 8.0F) {
        dst_reg[3] = 25.0; // double
    }
    v_endif;

    v_if(dst_reg[0] == 9.0F) {
        vFloat a = 28.0; // double
        dst_reg[3] = a;
    }
    v_endif;

    v_if(dst_reg[0] == 62.0F) {
        // Store into [62] so the compared value isn't close to the expected value
        dst_reg[3] = vConst0p0020;
    }
    v_endif;

    // [0] = 10.0
    // [1] = 20.0
    // [2] = 30.0
    // [3] = 0x3f80
    // [4] = 0x3f80
    // [5] = 0xFFFF
    // [6] = 120.0F
    // [7] = 0xFFFF
    // [8] = 25.0
    // [9] = 28.0
    // [10] on 20.0F
    // except [62] = .001953

    copy_result_to_dreg0(3);
}

sfpi_test_noinline void test4()
{
    // Test SFPPUSHCC, SFPPOPCC, SFPMAD (in conditionals)
    // Test vector loads
    // Operators &&, ||, !

    vFloat v = dst_reg[0];

    dst_reg[4] = v;

    v_if(v < 2.0F) {
        dst_reg[4] = 64.0F;
    }
    v_endif;
    // [0,1] = 64.0

    v_if(v < 6.0F) {
        v_if(v >= 2.0F) {
            v_if(v >= 3.0F) {
                dst_reg[4] = 65.0F;
            } v_else {
                dst_reg[4] = 66.0F;
            }
            v_endif;

            v_if(v == 5.0F) {
                dst_reg[4] = 67.0F;
            }
            v_endif;
        }
        v_endif;
    }
    v_endif;
    // [2] = 66.0
    // [3, 4] = 65.0
    // [5] = 67.0

    v_if(v >= 6.0F) {
        v_if(v < 9.0F) {
            v_if(v == 6.0F) {
                dst_reg[4] = 68.0F;
            } v_elseif(v != 8.0F) {
                dst_reg[4] = 69.0F;
            } v_else {
                dst_reg[4] = 70.0F;
            }
            v_endif;
        } v_elseif(v == 9.0F) {
            dst_reg[4] = 71.0F;
        } v_elseif(v == 10.0F) {
            dst_reg[4] = 72.0F;
        }

        v_endif;
    }
    v_endif;

    v_if(v >= 11.0F) {
        v_if(v < 18.0F && v >= 12.0F && v != 15.0F) {
            dst_reg[4] = 120.0F;
        } v_else {
            dst_reg[4] = -dst_reg[0];
        }
        v_endif;
    }
    v_endif;

    v_if(v >= 18.0F && v < 23.0F) {
        v_if(v == 19.0F || v == 21.0F) {
            dst_reg[4] = 160.0F;
        } v_else {
            dst_reg[4] = 180.0F;
        }
        v_endif;
    }
    v_endif;

    // Test ! on OP
    v_if(!(v != 23.0F)) {
        dst_reg[4] = 200.0F;
    }
    v_endif;

    v_if(!(v >= 25.0F) && !(v < 24.0F)) {
        dst_reg[4] = 220.0F;
    }
    v_endif;

    // Test ! on Boolean
    v_if(!((v < 25.0F) || (v >= 26.0F))) {
        dst_reg[4] = 240.0F;
    }
    v_endif;

    v_if((v >= 26.0F) && (v < 29.0F)) {
        dst_reg[4] = 260.0F;
        v_if(!((v >= 27.0F) && (v < 28.0F))) {
            dst_reg[4] = 270.0F;
        }
        v_endif;
    }
    v_endif;

    // Test || after && to be sure PUSHC works properly
    v_if ((v >= 28.0F) && (v == 29.0F || v == 30.0F || v == 31.0F)) {
        vFloat x = 30.0F;
        vFloat y = 280.0F;
        v_if (v < x) {
            y += 10.0F;
        }
        v_endif;
        v_if (v == x) {
            y += 20.0F;
        }
        v_endif;
        v_if (v >= x) {
            y += 40.0F;
        }
        v_endif;
        dst_reg[4] = y;
    }
    v_endif;

    // [7] = 69.0
    // [8] = 70.0
    // [9] = 71.0
    // [10] = 72.0
    // [11] = -11.0
    // [12] = 120.0
    // [13] = 120.0
    // [14] = 120.0
    // [15] = -15.0
    // [16] = 120.0
    // [17] = 120.0
    // [18] = 180.0
    // [19] = 160.0
    // [20] = 180.0
    // [21] = 160.0
    // [22] = 180.0
    // [23] = 200.0
    // [24] = 220.0
    // [25] = 240.0
    // [26] = 270.0
    // [27] = 260.0
    // [28] = 270.0
    // [29] = 290.0
    // [30] = 340.0
    // [31] = 320.0

    // Remainder is -ramp
    copy_result_to_dreg0(4);
}

sfpi_test_noinline void test5()
{
    // Test SFPMAD, SFPMOV, vConsts
    dst_reg[5] = -dst_reg[0];

    v_if(dst_reg[0] == 0.0F) {
        dst_reg[5] = vConst0 * vConst0 + vConst0p6929;
    } v_elseif(dst_reg[0] == 1.0F) {
        dst_reg[5] = vConst0 * vConst0 + vConst0;
    } v_elseif(dst_reg[0] == 2.0F) {
        dst_reg[5] = vConst0 * vConst0 + vConstNeg1p0068;
    } v_elseif(dst_reg[0] == 3.0F) {
        dst_reg[5] = vConst0 * vConst0 + vConst1p4424;
    } v_elseif(dst_reg[0] == 4.0F) {
        dst_reg[5] = vConst0 * vConst0 + vConst0p8369;
    } v_elseif(dst_reg[0] == 5.0F) {
        dst_reg[5] = vConst0 * vConst0 + vConstNeg0p5;
    } v_elseif(dst_reg[0] == 6.0F) {
        dst_reg[5] = vConst0 * vConst0 + vConst1;
    }
    v_endif;
    // [0] = 0.0
    // [1] = 0.692871094
    // [2] = -1.00683594
    // [3] = 1.442382813
    // [4] = 0.836914063
    // [5] = -0.5
    // [6] = 1.0

    v_if(dst_reg[0] == 7.0F) {
        dst_reg[5] = vConst0 * vConst0 + vConstNeg1;
    } v_elseif(dst_reg[0] == 8.0F) {
        dst_reg[5] = vConst0 * vConst0 + vConst0p0020;
    } v_elseif(dst_reg[0] == 9.0F) {
        dst_reg[5] = vConst0 * vConst0 + vConstNeg0p6748;
    } v_elseif(dst_reg[0] == 10.0F) {
        dst_reg[5] = vConst0 * vConst0 + vConstNeg0p3447;
    } v_elseif(dst_reg[0] == 11.0F) {
        dst_reg[5] = vConstNeg0p6748 * vConstNeg0p3447 + vConstNeg1;
    }
    v_endif;
    // [7] = -1.0
    // [8] = 0.001953125
    // [9] = -0.67480469
    // [10] = -0.34472656
    // [11] = -0.765625

    vFloat a = dst_reg[0];
    vFloat b = 20.0F;

    // Note: loading dst_reg[0] takes a reg and comparing against a float const
    // takes a reg so can't store A, B and C across the condtionals

    v_if(dst_reg[0] == 12.0F) {
        dst_reg[5] = a * b;
    } v_elseif(dst_reg[0] == 13.0F) {
        dst_reg[5] = a + b;
    } v_elseif(dst_reg[0] == 14.0F) {
        dst_reg[5] = a * b + 0.5F;
    } v_elseif(dst_reg[0] == 15.0F) {
        dst_reg[5] = a + b + 0.5F;
    } v_elseif(dst_reg[0] == 16.0F) {
        dst_reg[5] = a * b - 0.5F;
    } v_elseif(dst_reg[0] == 17.0F) {
        dst_reg[5] = a + b - 0.5F;
    } v_elseif(dst_reg[0] == 18.0F) {
        vFloat c = -5.0F;
        dst_reg[5] = a * b + c;
    }
    v_endif;
    // [12] = 240.0
    // [13] = 33.0
    // [14] = 280.5
    // [15] = 35.5
    // [16] = 319.5
    // [17] = 36.5
    // [18] = 355.0

    v_if(dst_reg[0] == 19.0F) {
        vFloat c = -5.0F;
        dst_reg[5] = a * b + c + 0.5F;
    } v_elseif(dst_reg[0] == 20.0F) {
        vFloat c = -5.0F;
        dst_reg[5] = a * b + c - 0.5F;
    } v_elseif(dst_reg[0] == 21.0F) {
        vFloat c = -5.0F;
        vFloat d;
        d = a * b + c - 0.5F;
        dst_reg[5] = d;
    } v_elseif(dst_reg[0] == 22.0F) {
        vFloat c = -5.0F;
        dst_reg[5] = a * b - c;
    } v_elseif(dst_reg[0] == 23.0F) {
        dst_reg[5] = a * b + vConstNeg1;
    } v_elseif(dst_reg[0] == 24.0F) {
        dst_reg[5] = vConstNeg1 * b + vConstNeg1;
    }
    v_endif;
    // [19] = 375.5
    // [20] = 394.5
    // [21] = 414.5
    // [22] = 445.0
    // [23] = 459.0
    // [24] = -21.0

    v_if(dst_reg[0] == 25.0F) {
        vFloat c = -5.0F;
        dst_reg[5] = dst_reg[0] * b + c;
    } v_elseif(dst_reg[0] == 26.0F) {
        vFloat c = -5.0F;
        dst_reg[5] = b * dst_reg[0] + c;
    } v_elseif(dst_reg[0] == 27.0F) {
        dst_reg[5] = a * b + dst_reg[0];
    } v_elseif(dst_reg[0] == 28.0F) {
        dst_reg[5] = a * b - dst_reg[0];
    }
    v_endif;
    // [25] = 495.0
    // [26] = 515.0
    // [27] = 567.0
    // [28] = 532.0

    v_if(dst_reg[0] == 29.0F) {
        dst_reg[5] = a - b;
    } v_elseif(dst_reg[0] == 30.0F) {
        dst_reg[5] = a - b - 0.5F;
    } v_elseif(dst_reg[0] == 31.0F) {
        dst_reg[5] = dst_reg[0] - b + 0.5F;
    }
    v_endif;
    // [29] = 9.0
    // [30] = 9.5
    // [31] = 11.5
    // [32+] = ramp

    copy_result_to_dreg0(5);
}

sfpi_test_noinline void test6()
{
    // Note: set_expected_result uses SFPIADD so can't really be used early in
    // this routine w/o confusing things

    // SFPIADD

    dst_reg[6] = -dst_reg[0];

    v_if(dst_reg[0] < 3.0F) {
        v_if(dst_reg[0] >= 0.0F) {

            dst_reg[6] = 256.0F;

            vInt a;
            v_if(dst_reg[0] == 0.0F) {
                a = 28;
            } v_elseif(dst_reg[0] == 1.0F) {
                a = 29;
            } v_elseif(dst_reg[0] == 2.0F) {
                a = 30;
            }
            v_endif;

            vInt b;
            // IADD imm
            b = a - 29;
            v_if(b >= 0) {
                dst_reg[6] = 1024.0F;
            }
            v_endif;
        }
        v_endif;
    }
    v_endif;

    v_if(dst_reg[0] < 6.0F) {
        v_if(dst_reg[0] >= 3.0F) {
            dst_reg[6] = 256.0F;

            vInt a;
            v_if(dst_reg[0] == 3.0F) {
                a = 28;
            } v_elseif(dst_reg[0] == 4.0F) {
                a = 29;
            } v_elseif(dst_reg[0] == 5.0F) {
                a = 30;
            }
            v_endif;

            vInt b = -29;
            // IADD reg
            b = a + b;
            v_if(b < 0) {
                dst_reg[6] = 1024.0F;
            }
            v_endif;
        }
        v_endif;
    }
    v_endif;

    v_if(dst_reg[0] < 9.0F) {
        v_if(dst_reg[0] >= 6.0F) {
            dst_reg[6] = 16.0F;

            vInt a = 3;
            v_if(dst_reg[0] == 6.0F) {
                a = 28;
            } v_elseif(dst_reg[0] == 7.0F) {
                a = 29;
            } v_elseif(dst_reg[0] == 8.0F) {
                a = 30;
            }
            v_endif;

            vFloat b = 128.0F;
            v_if(a >= 29) {
                b = 256.0F;
            }
            v_endif;

            v_if(a < 29) {
                b = 512.0F;
            } v_elseif(a >= 30) {
                b = 1024.0F;
            }
            v_endif;

            dst_reg[6] = b;
        }
        v_endif;
    }
    v_endif;

    v_if(dst_reg[0] < 12.0F) {
        v_if(dst_reg[0] >= 9.0F) {
            dst_reg[6] = 16.0F;

            vInt a = 3;
            v_if(dst_reg[0] == 9.0F) {
                a = 28;
            } v_elseif(dst_reg[0] == 10.0F) {
                a = 29;
            } v_elseif(dst_reg[0] == 11.0F) {
                a = 30;
            }
            v_endif;

            vFloat b = 128.0F;
            vInt c = 29;
            v_if(a >= c) {
                b = 256.0F;
            }
            v_endif;

            v_if(a < c) {
                b = 512.0F;
            } v_elseif(a >= 30) {
                b = 1024.0F;
            }
            v_endif;

            dst_reg[6] = b;
        }
        v_endif;
    }
    v_endif;

    v_if (dst_reg[0] == 12.0F) {
        vInt v = 25;
        set_expected_result(6, 4.0F, 25, v);
    } v_elseif(dst_reg[0] == 13.0F) {
        vInt a = 20;
        a = a + 12;
        set_expected_result(6, 8.0F, 32, a);
    } v_elseif(dst_reg[0] == 14.0F) {
        vInt a = 18;
        vInt b = -6;
        a = a + b;
        set_expected_result(6, 16.0F, 12, a);
    } v_elseif(dst_reg[0] == 15.0F) {
        vInt a = 14;
        vInt b = -5;
        a = b + a;
        set_expected_result(6, 32.0F, 9, a);
    }
    v_endif;

    v_if (dst_reg[0] == 16.0F) {
        vInt v = 25;
        set_expected_result(6, 4.0F, 25, v);
    } v_elseif(dst_reg[0] == 17.0F) {
        vInt a = 20;
        a = a - 12;
        set_expected_result(6, 8.0F, 8, a);
    } v_elseif(dst_reg[0] == 18.0F) {
        vInt a = 18;
        vInt b = 6;
        a = a - b;
        set_expected_result(6, 16.0F, 12, a);
    } v_elseif(dst_reg[0] == 19.0F) {
        vInt a = 14;
        vInt b = 5;
        a = b - a;
        set_expected_result(6, 32.0F, -9, a);
    }
    v_endif;

    v_if (dst_reg[0] == 20.0F) {
        vUInt v = 25;
        set_expected_result(6, 4.0F, 25, reinterpret<vInt>(v));
    } v_elseif(dst_reg[0] == 21.0F) {
        vUInt a = 20;
        a = a - 12;
        set_expected_result(6, 8.0F, 8, reinterpret<vInt>(a));
    } v_elseif(dst_reg[0] == 22.0F) {
        vUInt a = 18;
        vUInt b = 6;
        a = a - b;
        set_expected_result(6, 16.0F, 12, reinterpret<vInt>(a));
    } v_elseif(dst_reg[0] == 23.0F) {
        vUInt a = 14;
        vUInt b = 5;
        a = b - a;
        set_expected_result(6, 32.0F, -9, reinterpret<vInt>(a));
    }
    v_endif;

    v_if (dst_reg[0] == 24.0F) {
        vInt a = 10;
        vInt b = 20;
        a -= b;
        set_expected_result(6, 64.0F, -10, a);
    } v_elseif (dst_reg[0] == 25.0F) {
        vInt a = 10;
        vInt b = 20;
        a += b;
        set_expected_result(6, 128.0F, 30, a);
    }
    v_endif;

    // Pseudo-16 bit via hidden loadi
    v_if (dst_reg[0] == 26.0F) {
        vInt a = 10;
        a += 4096;
        set_expected_result(6, 256.0F, 4106, a);
    } v_elseif (dst_reg[0] == 27.0F) {
        vInt a = 4096;
        v_if (a >= 4096) {
            dst_reg[6] = 512.0f;
        } v_else {
            dst_reg[6] = 0.0f;
        }
        v_endif;
    }
    v_endif;

    v_if (dst_reg[0] >= 28.0F) {
        vInt a = vConstTileId;
        v_if (dst_reg[0] == 28.0F) {
            set_expected_result(6, 256.0F, 28, a);
        } v_elseif (dst_reg[0] == 29.0F) {
            set_expected_result(6, 256.0F, 29, a);
        } v_elseif (dst_reg[0] == 30.0F) {
            set_expected_result(6, 256.0F, 30, a);
        }
        v_endif;
    }
    v_endif;

    v_if (dst_reg[0] == 31.0F) {
        vFloat x = 3.0f;
        v_if (!!(x == 3.0f && x != 4.0f)) {
            dst_reg[6] = 16.0;
        } v_else {
            dst_reg[6] = 32.0;
        }
        v_endif;
    }
    v_endif;

    // [0] = 256.0
    // [1] = 1024.0
    // [2] = 1024.0
    // [3] = 1024.0
    // [4] = 256.0
    // [5] = 256.0
    // [6] = 512.0
    // [7] = 256.0
    // [8] = 1024.0
    // [9] = 512.0
    // [10] = 256.0
    // [11] = 1024.0
    // [12] = 4.0
    // [13] = 8.0
    // [14] = 16.0
    // [15] = 32.0
    // [16] = 4.0
    // [17] = 8.0
    // [18] = 16.0
    // [19] = 32.0
    // [20] = 4.0
    // [21] = 8.0
    // [22] = 16.0
    // [23] = 32.0
    // [24] = 64.0
    // [25] = 128.0
    // [26] = 256.0
    // [27] = 512.0
    // [28] = 256.0
    // [29] = 256.0
    // [30] = 256.0

    copy_result_to_dreg0(6);
}

sfpi_test_noinline void test7()
{
    // SFPEXMAN, SFPEXEXP, SFPSETEXP, SFPSETMAN
    // Plus a little more && ||

    dst_reg[7] = -dst_reg[0];
    v_if(dst_reg[0] == 1.0F) {
        vFloat tmp = 124.0F;
        set_expected_result(7, 30.0F, 0x7C0, exman8(tmp));
    } v_elseif(dst_reg[0] == 2.0F) {
        vFloat tmp = 124.0F;
        set_expected_result(7, 32.0F, 0x3C0, exman9(tmp));
    } v_elseif(dst_reg[0] == 3.0F) {
        vFloat tmp = 65536.0F * 256.0F;
        set_expected_result(7, 33.0F, 0x18, exexp(tmp));
    } v_elseif(dst_reg[0] == 4.0F) {
        vFloat tmp = 65536.0F * 256.0F;
        set_expected_result(7, 34.0F, 0x97, exexp_nodebias(tmp));
    } v_elseif(dst_reg[0] < 8.0F) {
        vFloat tmp;
        v_if(dst_reg[0] == 5.0F) {
            // Exp < 0 for 5.0
            tmp = 0.5F;
        } v_elseif(dst_reg[0] < 8.0F) {
            // Exp > 0 for 6.0, 7.0
            tmp = 512.0F;
        }
        v_endif;

        vInt v;
        v = exexp(tmp);
        v_if(v < 0) {
            dst_reg[7] = 32.0F;
        } v_else {
            dst_reg[7] = 64.0F;
        }
        v_endif;

        v_if (dst_reg[0] == 7.0F) {
            // Exponent is 9, save it
            set_expected_result(7, 35.0F, 9, v);
        }
        v_endif;
        // [0] = 64.0
        // [1] = 30.0
        // [2] = 32.0
        // [3] = 33.0
        // [4] = 34.0
        // [5] = 32.0
        // [6] = 64.0
        // [7] = 35.0 (exponent(512) = 8)
    } v_elseif(dst_reg[0] == 8.0F) {
        vFloat tmp = 1.0F;
        vFloat v = setexp(tmp, 137);
        dst_reg[7] = v;
    } v_elseif(dst_reg[0] == 9.0F) {
        vInt exp = 0x007F; // Exponent in low bits
        vFloat sgn_man = -1664.0F; // 1024 + 512 + 128 or 1101
        sgn_man = setexp(sgn_man, exp);
        dst_reg[7] = sgn_man;
    }
    v_endif;

    // [8] = 1024.0
    // [9] = -1.625

    v_if(dst_reg[0] == 10.0F) {
        vFloat tmp = 1024.0F;
        vFloat b = setman(tmp, 0x3AB);
        dst_reg[7] = b;
    } v_elseif(dst_reg[0] == 11.0F) {
        vFloat tmp = 1024.0F;
        vInt man = 0xBBB;
        vFloat tmp2 = setman(tmp, man);
        dst_reg[7] = tmp2;
    }
    v_endif;

    // [10] = 1960.0 (?)
    // [11] = 1024.0

    vFloat v = dst_reg[0];
    v_if ((v >= 12.0f && v < 14.0f) || (v >= 15.0f && v < 17.0f)) {
        dst_reg[7] = -128.0f;
    }
    v_endif;
    // [12] = -128.0
    // [13] = -128.0
    // [14] = -14.0
    // [15] = -128.0
    // [16] = -128.0

    v_if(((v >= 17.0f && v < 18.0f) || (v >= 19.0f && v < 20.0f)) ||
         ((v >= 21.0f && v < 22.0f) || (v >= 23.0f && v < 24.0f))) {
        dst_reg[7] = -256.0f;
    }
    v_endif;
    // [17] = -256.0
    // [18] = -18.0
    // [19] = -256.0
    // [20] = -20.0
    // [21] = -256.0
    // [22] = -22.0
    // [23] = -256.0
    // [24] = -24.0

    v_if (v >= 25.0f && v < 29.0f) {
        v_if(!(v >= 25.0f && v < 26.0f) && !(v >= 27.0f && v < 28.0f)) {
            dst_reg[7] = -1024.0f;
        }
        v_endif;
    }
    v_endif;
    // [25] = -25.0
    // [26] = -1024.0
    // [27] = -27.0
    // [28] = -1024.0

    // <= and > are compound statements in the compiler, <= uses a compc
    // and things get flipped around when joined by ||
    v_if (v >= 29.0f && v < 32.0f) {
        vInt t = vConstTileId;
        vFloat total = 16.0F;

        v_if (t <= 30) {
            total += 32.0F;
        }
        v_endif;
        v_if (t > 30) {
            total += 64.0F;
        }
        v_endif;
        v_if (!(t > 30)) {
            total += 128.0F;
        }
        v_endif;
        v_if (!(t <= 30)) {
            total += 256.0F;
        }
        v_endif;
        v_if (t <= 29 || t > 30) {
            total += 512.0F;
        }
        v_endif;
        v_if (t > 30 || t <= 29) {
            total += 1024.0F;
        }
        v_endif;

        dst_reg[7] = total;
    }
    v_endif;
    // [29] = 1712.0
    // [30] = 176.0
    // [31] = 1872.0

    copy_result_to_dreg0(7);
}

sfpi_test_noinline void test8()
{
    // SFPAND, SFPOR, SFPNOT, XOR, SFPABS
    // Atypical usage of conditionals
    // More conditionals (short v compares)

    dst_reg[8] = -dst_reg[0];
    v_if(dst_reg[0] == 1.0F) {
        vUInt a = 0x05FF;
        vUInt b = 0x0AAA;
        b &= a;
        set_expected_result(8, 16.0F, 0x00AA, static_cast<vInt>(b));
    } v_elseif(dst_reg[0] == 2.0F) {
        vUInt a = 0x05FF;
        vUInt b = 0x0AAA;
        vUInt c = a & b;
        set_expected_result(8, 16.0F, 0x00AA, static_cast<vInt>(c));
    } v_elseif(dst_reg[0] == 3.0F) {
        vInt a = 0x05FF;
        vInt b = 0x0AAA;
        b &= a;
        set_expected_result(8, 16.0F, 0x00AA, b);
    } v_elseif(dst_reg[0] == 4.0F) {
        vInt a = 0x05FF;
        vInt b = 0x0AAA;
        vInt c = a & b;
        set_expected_result(8, 16.0F, 0x00AA, c);
    }
    v_endif;

    v_if(dst_reg[0] == 5.0F) {
        vUInt a = 0x0111;
        vUInt b = 0x0444;
        b |= a;
        set_expected_result(8, 20.0F, 0x0555, static_cast<vInt>(b));
    } v_elseif(dst_reg[0] == 6.0F) {
        vUInt a = 0x0111;
        vUInt b = 0x0444;
        vUInt c = b | a;
        set_expected_result(8, 20.0F, 0x0555, static_cast<vInt>(c));
    } v_elseif(dst_reg[0] == 7.0F) {
        vInt a = 0x0111;
        vInt b = 0x0444;
        b |= a;
        set_expected_result(8, 20.0F, 0x0555, b);
    } v_elseif(dst_reg[0] == 8.0F) {
        vInt a = 0x0111;
        vInt b = 0x0444;
        vInt c = b | a;
        set_expected_result(8, 20.0F, 0x0555, c);
    }
    v_endif;

    v_if(dst_reg[0] == 9.0F) {
        vUInt a = 0x0AAA;
        a = ~a;
        a &= 0x0FFF; // Tricky since ~ flips upper bits that immediates can't access
        set_expected_result(8, 22.0F, 0x0555, static_cast<vInt>(a));
    } v_elseif(dst_reg[0] == 10.0F) {
        vFloat a = 100.0F;
        dst_reg[8] = sfpi::abs(a);
    } v_elseif(dst_reg[0] == 11.0F) {
        vFloat a = -100.0F;
        dst_reg[8] = sfpi::abs(a);
    } v_elseif(dst_reg[0] == 12.0F) {
        vInt a = 100;
        set_expected_result(8, 24.0F, 100, sfpi::abs(a));
    } v_elseif(dst_reg[0] == 13.0F) {
        vInt a = -100;
        set_expected_result(8, 26.0F, 100, sfpi::abs(a));
    }
    v_endif;

    v_if (test_interleaved_scalar_vector_cond(true, dst_reg[0], 14.0F, 15.0F)) {
        dst_reg[8] = 32.0F;
    } v_elseif(test_interleaved_scalar_vector_cond(false, dst_reg[0], 14.0F, 15.0F)) {
        dst_reg[8] = 16.0F;
    }
    v_endif;

    vFloat tmp = dst_reg[8];
    v_block {
        v_and(dst_reg[0] >= 16.0F);

        for (int x = 0; x < 4; x++) {
            v_and(dst_reg[0] < 20.0F - x);
            tmp += 16.0F;
        }
    }
    v_endblock;
    dst_reg[8] = tmp;

    // <= and > are compound statements in the compiler, <= uses a compc
    // and things get flipped around when joined by ||
    v_if (dst_reg[0] >= 20.0f && dst_reg[0] < 23.0f) {
        vInt t = vConstTileId;
        vInt low = 20;
        vInt high = 21;

        dst_reg[8] = 16.0f;

        v_if (t <= high) {
            dst_reg[8] = dst_reg[8] + 32.0F;
        }
        v_endif;
        v_if (t > high) {
            dst_reg[8] = dst_reg[8] + 64.0F;
        }
        v_endif;
        v_if (!(t > high)) {
            dst_reg[8] = dst_reg[8] + 128.0F;
        }
        v_endif;
        v_if (!(t <= high)) {
            dst_reg[8] = dst_reg[8] + 256.0F;
        }
        v_endif;
        v_if (t <= low || t > high) {
            dst_reg[8] = dst_reg[8] + 512.0F;
        }
        v_endif;
        v_if (t > high || t <= low) {
            dst_reg[8] = dst_reg[8] + 1024.0F;
        }
        v_endif;
    }
    v_endif;

    // Do the same tests as above, but for floats
    v_if (dst_reg[0] >= 23.0f && dst_reg[0] < 26.0f) {
        vFloat t = dst_reg[0];
        vFloat total = 16.0F;

        v_if (t <= 24.0f) {
            total += 32.0F;
        }
        v_endif;
        v_if (t > 24.0f) {
            total += 64.0F;
        }
        v_endif;
        v_if (!(t > 24.0f)) {
            total += 128.0F;
        }
        v_endif;
        v_if (!(t <= 24.0f)) {
            total += 256.0F;
        }
        v_endif;
        v_if (t <= 23.0f || t > 24.0f) {
            total += 512.0F;
        }
        v_endif;
        v_if (t > 24.0f || t <= 23.0f) {
            total += 1024.0F;
        }
        v_endif;

        dst_reg[8] = total;
    }
    v_endif;

    // More of the same, again for floats.  Reloads for reg pressure
    v_if (dst_reg[0] >= 26.0f && dst_reg[0] < 29.0f) {
        vFloat low = 26.0f;
        vFloat high = 27.0f;

        dst_reg[8] = 16.0f;

        vFloat t = dst_reg[0];
        v_if (t <= high) {
            dst_reg[8] = dst_reg[8] + 32.0F;
        }
        v_endif;
        t = dst_reg[0];
        v_if (t > high) {
            dst_reg[8] = dst_reg[8] + 64.0F;
        }
        v_endif;
        t = dst_reg[0];
        v_if (!(t > high)) {
            dst_reg[8] = dst_reg[8] + 128.0F;
        }
        v_endif;
        t = dst_reg[0];
        v_if (!(t <= high)) {
            dst_reg[8] = dst_reg[8] + 256.0F;
        }
        v_endif;
        t = dst_reg[0];
        v_if (t <= low || t > high) {
            dst_reg[8] = dst_reg[8] + 512.0F;
        }
        v_endif;
        t = dst_reg[0];
        low = 26.0f;
        v_if (t > high || t <= low) {
            dst_reg[8] = dst_reg[8] + 1024.0F;
        }
        v_endif;
    }
    v_endif;

    v_if(dst_reg[0] == 29.0F) {
        vInt a = 0xA5A5;
        vInt b = 0xFF00;
        vInt c = a ^ b;
        set_expected_result(8, 32.0F, 0x5AA5, c);
    }
    v_endif;

    v_if(dst_reg[0] == 30.0F) {
        vUInt a = 0xA5A5;
        vUInt b = 0xFF00;
        vUInt c = a ^ b;
        set_expected_result(8, 64.0F, 0x5AA5, c);
    }
    v_endif;

    v_if(dst_reg[0] == 31.0F) {
        vInt a = 0xA5A5;
        vInt b = 0xFF00;
        b ^= a;
        set_expected_result(8, 32.0F, 0x5AA5, b);
    }
    v_endif;

    // [0] = 0
    // [1] = 16.0
    // [2] = 16.0
    // [3] = 16.0
    // [4] = 16.0
    // [5] = 20.0
    // [6] = 20.0
    // [7] = 20.0
    // [8] = 20.0
    // [9] = 22.0
    // [10] = 100.0
    // [11] = 100.0
    // [12] = 24.0
    // [13] = 26.0
    // [14] = 32.0
    // [15] = 16.0
    // [16] = 48.0
    // [17] = 31.0
    // [18] = 14.0
    // [19] = -3.0
    // [20] = 1712.0
    // [21] = 176.0
    // [22] = 1872.0
    // [23] = 1712.0
    // [24] = 176.0
    // [25] = 1872.0
    // [26] = 1712.0
    // [27] = 176.0
    // [28] = 1872.0
    // [29] = 32.0
    // [30] = 64.0
    // [31] = 32.0

    copy_result_to_dreg0(8);
}

sfpi_test_noinline void test9()
{
    // SFPMULI, SFPADDI, SFPDIVP2, SFPLZ
    // More conditional tests

    dst_reg[9] = -dst_reg[0];
    v_if(dst_reg[0] == 1.0F) {
        vFloat a = 20.0F;
        dst_reg[9] = a * 30.0F;
    } v_elseif(dst_reg[0] == 2.0F) {
        vFloat a = 20.0F;
        a *= 40.0F;
        dst_reg[9] = a;
    } v_elseif(dst_reg[0] == 3.0F) {
        vFloat a = 20.0F;
        dst_reg[9] = a + 30.0F;
    } v_elseif(dst_reg[0] == 4.0F) {
        vFloat a = 20.0F;
        a += 40.0F;
        dst_reg[9] = a;
    } v_elseif(dst_reg[0] == 5.0F) {
        vFloat a = 16.0F;
        dst_reg[9] = addexp(a, 4);
    } v_elseif(dst_reg[0] == 6.0F) {
        vFloat a = 256.0F;
        dst_reg[9] = addexp(a, -4);
    }
    v_endif;

    v_if(dst_reg[0] == 7.0F) {
        vInt a = 0;
        vInt b = lz(a);
        set_expected_result(9, 38.0F, 0x13, b);
    } v_elseif(dst_reg[0] == 8.0F) {
        vInt a = 0xFFFF;
        vInt b = lz(a);
        set_expected_result(9, 55.0F, 0x0, b);
    } v_elseif(dst_reg[0] == 9.0F) {
        vUInt a = 0xFFFFU;
        vInt b = lz(a);
        set_expected_result(9, 30.0F, 0x3, b);
    } v_elseif(dst_reg[0] < 13.0F) {
        vFloat a = dst_reg[0] - 11.0F;
        vUInt b;

        // Relies on if chain above...
        v_if(dst_reg[0] >= 7.0F) {
            b = reinterpret<vUInt>(lz(a));
            v_if (b != 19) {
                dst_reg[9] = 60.0F;
            } v_else {
                dst_reg[9] = 40.0F;
            }
            v_endif;
        }
        v_endif;
    }
    v_endif;

    v_if(dst_reg[0] == 13.0F) {
        vFloat x = 1.0F;

        x *= 2.0f;
        x *= -3.0f;
        x += 4.0f;
        x += -4.0f;

        dst_reg[9] = x;
    } v_elseif(dst_reg[0] == 14.0F) {
        // MULI/ADDI don't accept fp16a
        // Ensure this goes to MAD

        vFloat x = 1.0F;

        x *= s2vFloat16a(2.0);
        x *= s2vFloat16a(-3.0);
        x += s2vFloat16a(4.0);
        x += s2vFloat16a(-4.0);

        dst_reg[9] = x;
    }
    v_endif;

    // Test more boolean expressions
    v_if (dst_reg[0] >= 15.0F && dst_reg[0] < 19.0) {
        vFloat v = dst_reg[0];
        v_if ((v <= 16.0f && v != 15.0f) || (v == 18.0f)) {
            dst_reg[9] = 32.0f;
        }
        v_endif;
    }
    v_endif;

    // Same as above, but flip the order of the top level OR
    v_if (dst_reg[0] >= 19.0F && dst_reg[0] < 23.0) {
        vFloat v = dst_reg[0];
        v_if ((v == 22.0f) || (v <= 20.0f && v != 19.0f)) {
            dst_reg[9] = 32.0f;
        }
        v_endif;
    }
    v_endif;

    v_if ((dst_reg[0] == 23.0 || dst_reg[0] == 24.0 ||
           dst_reg[0] == 25.0 || dst_reg[0] == 26.0 ||
           dst_reg[0] == 27.0 || dst_reg[0] == 28.0) &&
          (dst_reg[0] != 23.0 && dst_reg[0] != 25.0 && dst_reg[0] != 27.0f)) {
        dst_reg[9] = 64.0f;
    }
    v_endif;

    // [1] = 600.0
    // [2] = 800.0
    // [3] = 50.0
    // [4] = 60.0
    // [5] = 256.0
    // [6] = 16.0
    // [7] = 38.0
    // [8] = 55.0
    // [9] = 30.0
    // [10] = 60.0
    // [11] = 40.0
    // [12] = 60.0
    // [13] = -6.0
    // [14] = -6.0
    // [15] = -15.0
    // [16] = 32.0
    // [17] = -17.0
    // [18] = 32.0
    // [19] = -19.0
    // [20] = 32.0
    // [21] = -21.0
    // [22] = 32.0
    // [23] = -23.0
    // [24] = 64.0
    // [25] = -25.0
    // [26] = 64.0
    // [27] = -27.0
    // [28] = 64.0

    copy_result_to_dreg0(9);
}

sfpi_test_noinline void test10()
{
    // SFPSHFT, SFTSETSGN
    dst_reg[10] = -dst_reg[0];
    v_if(dst_reg[0] == 1.0F) {
        vUInt a = 0x015;
        vInt shift = 6;
        vUInt b = shft(a, shift);
        // Could write better tests if we could return and test the int result
        set_expected_result(10, 20.0F, 0x0540, static_cast<vInt>(b));
    } v_elseif(dst_reg[0] == 2.0F) {
        vUInt a = 0x2AAA;
        vUInt b = shft(a, -4);
        set_expected_result(10, 22.0F, 0x02AA, static_cast<vInt>(b));
    } v_elseif(dst_reg[0] == 3.0F) {
        vUInt a = 0xAAAAU;
        vInt shift = -6;
        vUInt b = shft(a, shift);
        set_expected_result(10, 24.0F, 0x02AA, static_cast<vInt>(b));
    } v_elseif(dst_reg[0] == 4.0F) {
        vUInt a = 0x005A;
        vUInt b = shft(a, 4);
        set_expected_result(10, 26.0F, 0x05A0, static_cast<vInt>(b));
    } v_elseif(dst_reg[0] == 5.0F) {
        vInt a = 25;
        a = a + 5;
        a += 7;
        set_expected_result(10, 28.0F, 0x25, a);
    } v_elseif(dst_reg[0] == 6.0F) {
        vInt a = 28;
        vInt b = 100;
        a += b;
        set_expected_result(10, 30.0F, 0x80, a);
    }
    v_endif;

    v_if(dst_reg[0] == 7.0F) {
        vFloat a = dst_reg[0];
        dst_reg[10] = setsgn(a, 1);
    } v_elseif(dst_reg[0] == 8.0F) {
        vFloat a = dst_reg[0];
        vFloat b = -128.0;
        vFloat r = setsgn(b, a);

        dst_reg[10] = r;
    } v_elseif(dst_reg[0] == 9.0F) {
        vFloat a = -256.0F;
        dst_reg[10] = setsgn(a, 0);
    } v_elseif(dst_reg[0] == 10.0F) {
        vFloat a = dst_reg[0];
        a += 20.0f;
        vFloat b = -512.0F;
        vFloat r = setsgn(a, b);

        dst_reg[10] = r;
    }
    v_endif;

    // [1] = 20.0
    // [2] = 22.0
    // [3] = 24.0
    // [4] = 26.0
    // [5] = 28.0
    // [6] = 30.0
    // [7] = -7.0
    // [8] = 128.0
    // [9] = 256.0
    // [10] = -30.0
    copy_result_to_dreg0(10);
}

sfpi_test_noinline void test11()
{
    // SFPLUT, SFPLOADL<n>
    dst_reg[11] = -dst_reg[0];

    vUInt l0a = 0xFF30; // Multiply by 0.0, add 0.125
    vUInt l1a = 0X3020; // Multiply by 0.125, add 0.25
    v_if(dst_reg[0] == 1.0F) {
        // Use L0
        vFloat h = -0.3F;
        vUInt l2a = 0xA010; // Mulitply by -0.25, add 0.5
        h = lut_sign(h, l0a, l1a, l2a);
        dst_reg[11] = h;
    } v_elseif(dst_reg[0] == 2.0F) {
        // Use L0
        vFloat h = -0.3F;
        vUInt l2a = 0xA010; // Mulitply by -0.25, add 0.5
        h = lut(h, l0a, l1a, l2a);
        dst_reg[11] = h;
    } v_elseif(dst_reg[0] == 3.0F) {
        // Use L0
        vFloat h = -0.3F;
        vUInt l2a = 0xA010; // Mulitply by -0.25, add 0.5
        h = lut_sign(h, l0a, l1a, l2a, -1);
        dst_reg[11] = h;
    } v_elseif(dst_reg[0] == 4.0F) {
        // Use L0
        vFloat h = -0.3F;
        vUInt l2a = 0xA010; // Mulitply by -0.25, add 0.5
        h = lut(h, l0a, l1a, l2a, 1);
        dst_reg[11] = h;
    } v_elseif(dst_reg[0] == 5.0F) {
        // Use L1
        vFloat h = 1.0F;
        vUInt l2a = 0xA010; // Mulitply by -0.25, add 0.5
        h = lut(h, l0a, l1a, l2a, 1);
        dst_reg[11] = h;
    } v_elseif(dst_reg[0] == 6.0F) {
        // Use L2
        vFloat h = 4.0F;
        vUInt l2a = 0xA010; // Mulitply by -0.25, add 0.5
        h = lut_sign(h, l0a, l1a, l2a);
        dst_reg[11] = h;
    }
    v_endif;

    // Clear out the LUT, re-load it w/ ASM instructions, the pull it into
    // variables for the SFPLUT
    l0a = 0;
    l1a = 0;

    // These are fakedout w/ emule
    TTI_SFPLOADI(0, SFPLOADI_MOD0_USHORT, 0xFF20); // Mulitply by 0.0, add 0.25
    TTI_SFPLOADI(1, SFPLOADI_MOD0_USHORT, 0x2010); // Mulitply by 0.25, add 0.5
    vUInt l0b, l1b;
    l0b = l_reg[LRegs::LReg0];
    l1b = l_reg[LRegs::LReg0];

    v_if(dst_reg[0] == 7.0F) {
        // Use L0
        vFloat h = -0.3F;
        vUInt l2b = 0x9000;
        h = lut_sign(h, l0b, l1b, l2b);
        dst_reg[11] = h;
    } v_elseif(dst_reg[0] == 8.0F) {
        // Use L0
        vFloat h = -0.3F;
        vUInt l2b = 0x9000;
        h = lut(h, l0b, l1b, l2b);
        dst_reg[11] = h;
    } v_elseif(dst_reg[0] == 9.0F) {
        // Use L0
        vFloat h = -0.3F;
        vUInt l2b = 0x9000;
        h = lut_sign(h, l0b, l1b, l2b, -1);
        dst_reg[11] = h;
    } v_elseif(dst_reg[0] == 10.0F) {
        // Use L0
        vFloat h = -0.3F;
        vUInt l2b = 0x9000;
        h = lut(h, l0b, l1b, l2b, 1);
        dst_reg[11] = h;
    } v_elseif(dst_reg[0] == 11.0F) {
        // Use L1
        vFloat h = 1.0F;
        vUInt l2b = 0x9000;
        h = lut(h, l0b, l1b, l2b, 1);
        dst_reg[11] = h;
    } v_elseif(dst_reg[0] == 12.0F) {
        // Use L2
        vFloat h = 4.0F;
        vUInt l2b = 0x9000;
        h = lut_sign(h, l0b, l1b, l2b);
        dst_reg[11] = h;
    }
    v_endif;

    // [1] = 0.125
    // [2] = -0.125
    // [3] = -0.375
    // [4] = -0.625
    // [5] = 0.875
    // [6] = -0.5
    // [7] = 0.25
    // [8] = -0.25
    // [9] = -0.25
    // [10] = -0.75
    // [11] = 0.75
    // [12] = -1.0
    copy_result_to_dreg0(11);
}

sfpi_test_noinline void test12(int imm)
{
    // imm is 35
    // Test immediate forms of SFPLOAD, SFPLOADI, SFPSTORE, SFPIADD_I, SFPADDI
    // SFPMULI, SFPSHFT, SFPDIVP2, SFPSETEXP, SFPSETMAN, SFPSETSGN,
    // Tries to cover both positive and negative params (sign extension)
    dst_reg[12] = -dst_reg[0];

    v_if(dst_reg[0] == 1.0F) {
        dst_reg[12] = static_cast<float>(imm); // SFPLOADI
    } v_elseif(dst_reg[0] == 2.0F) {
        dst_reg[12] = static_cast<float>(-imm); // SFPLOADI
    } v_elseif(dst_reg[0] == 3.0F) {
        vInt a = 5;
        a += imm; // SFPIADD_I
        set_expected_result(12, 25.0F, 40, a);
    } v_elseif(dst_reg[0] == 4.0F) {
        vInt a = 5;
        a -= imm; // SFPIADD_I
        set_expected_result(12, -25.0F, -30, a);
    } v_elseif(dst_reg[0] == 5.0F) {
        vFloat a = 3.0F;
        a += static_cast<float>(imm); // SFPADDI
        dst_reg[12] = a;
    } v_elseif(dst_reg[0] == 6.0F) {
        vFloat a = 3.0F;
        a += static_cast<float>(-imm); // SFPADDI
        dst_reg[12] = a;
    }
    v_endif;

    v_if(dst_reg[0] == 7.0F) {
        vUInt a = 0x4000;
        a >>= imm - 25;
        set_expected_result(12, 64.0F, 0x0010, reinterpret<vInt>(a));
    } v_elseif(dst_reg[0] == 8.0F) {
        vUInt a = 1;
        a <<= imm - 25;
        set_expected_result(12, 128.0F, 0x0400, reinterpret<vInt>(a));
    } v_elseif(dst_reg[0] == 9.0F) {
        vFloat a = 256.0F;
        dst_reg[12] = addexp(a, imm - 31);
    } v_elseif(dst_reg[0] == 10.0F) {
        vFloat a = 256.0F;
        dst_reg[12] = addexp(a, imm - 39);
    }
    v_endif;

    v_if(dst_reg[0] == 11.0F) {
        vFloat a = 128.0;
        vFloat r = setsgn(a, imm - 36);
        dst_reg[12] = r;
    } v_elseif(dst_reg[0] == 12.0F) {
        vFloat tmp = 1024.0F;
        int man = 0xBBB + 35 - imm;
        vFloat tmp2 = setman(tmp, man);
        dst_reg[12] = tmp2;
    } v_elseif(dst_reg[0] == 13.0F) {
        int exp = 0x007F + 35 - imm; // Exponent in low bits
        vFloat sgn_man = -1664.0F; // 1024 + 512 + 128 or 1101
        sgn_man = setexp(sgn_man, exp);
        dst_reg[12] = sgn_man;
    }
    v_endif;

    dst_reg[30 + 35 - imm] = 30.0F; // SFPSTORE
    dst_reg[30 + 35 - imm + 1] = vConstNeg1;

    v_if(dst_reg[0] == 14.0F) {
        dst_reg[12] = dst_reg[30 + 35 - imm]; // SFPLOAD
    }
    v_endif;
    v_if(dst_reg[0] == 15.0F) {
        dst_reg[12] = dst_reg[30 + 35 - imm + 1]; // SFPLOAD
    }
    v_endif;

    // Test for store/load nops, imm store non-imm load
    // Need to use the semaphores to get TRISC to run ahead for non-imm loads

    v_if(dst_reg[0] == 16.0F) {
        // imm store, imm load
        vFloat a = 110.0F;

        TTI_SEMINIT(1, 0, p_stall::SEMAPHORE_3);
        TTI_SEMWAIT(p_stall::STALL_MATH, p_stall::SEMAPHORE_3, p_stall::STALL_ON_ZERO);

        dst_reg[12] = a;
        a = dst_reg[12];

        semaphore_post(3);

        dst_reg[12] = a + 1.0F;
    }
    v_endif;

    v_if(dst_reg[0] == 17.0F) {
        // imm store, non-imm load
        vFloat a = 120.0F;

        TTI_SEMINIT(1, 0, p_stall::SEMAPHORE_3);
        TTI_SEMWAIT(p_stall::STALL_MATH, p_stall::SEMAPHORE_3, p_stall::STALL_ON_ZERO);

        dst_reg[12] = a;
        a = dst_reg[imm - 23];

        semaphore_post(3);

        dst_reg[12] = a + 1.0F;
    }
    v_endif;

    v_if(dst_reg[0] == 18.0F) {
        // non-imm store, imm load
        vFloat a = 130.0F;

        TTI_SEMINIT(1, 0, p_stall::SEMAPHORE_3);
        TTI_SEMWAIT(p_stall::STALL_MATH, p_stall::SEMAPHORE_3, p_stall::STALL_ON_ZERO);

        dst_reg[imm - 23] = a;
        a = dst_reg[12];

        semaphore_post(3);

        dst_reg[12] = a + 1.0F;
    }
    v_endif;

    v_if(dst_reg[0] == 19.0F) {
        // non-imm store, non-imm load
        vFloat a = 140.0F;

        TTI_SEMINIT(1, 0, p_stall::SEMAPHORE_3);
        TTI_SEMWAIT(p_stall::STALL_MATH, p_stall::SEMAPHORE_3, p_stall::STALL_ON_ZERO);

        dst_reg[imm - 23] = a;
        a = dst_reg[imm - 23];

        semaphore_post(3);

        dst_reg[12] = a + 1.0F;
    }
    v_endif;

    v_if(dst_reg[0] == 20.0F) {
        vFloat a = 3.0F;
        a *= static_cast<float>(imm); // SFPADDI
        dst_reg[12] = a;
    } v_elseif(dst_reg[0] == 21.0F) {
        vFloat a = 3.0F;
        a *= static_cast<float>(-imm); // SFPADDI
        dst_reg[12] = a;
    }
    v_endif;

    // [1] = 35.0F
    // [2] = -35.0F
    // [3] = 25.0F
    // [4] = -25.0F
    // [5] = 38.0F
    // [6] = -32.0F
    // [7] = 64.0F
    // [8] = 128.0F
    // [9] = 4096.0F
    // [10] = 16.0F
    // [11] = -128.0F
    // [12] = 1976.0 // due to grayskull bug, other would be 1024.0F
    // [13] = -1.625
    // [14] = 30.0F
    // [15] = -1.0
    // [16] = 111.0F
    // [17] = 121.0F
    // [18] = 131.0F
    // [19] = 141.0F
    // [20] = 105.0F
    // [21] = -105.0F

    copy_result_to_dreg0(12);
}

// Test 13 covers variable liveness, ie, keeping a variable "alive" across a
// CC narrowing instruction.  Touches every affected instruction except LOAD,
// LOADI, IADD (those are covered in random tests above) across a SETCC
sfpi_test_noinline void test13(int imm)
{
    // Test variable liveness

    dst_reg[13] = -dst_reg[0];

    // ABS liveness across SETCC
    {
        vFloat x = -20.0F;
        vFloat y = -30.0F;
        v_if (dst_reg[0] == 0.0F) {
            y = sfpi::abs(x);
        }
        v_endif;
        v_if (dst_reg[0] == 0.0F || dst_reg[0] == 1.0F) {
            dst_reg[13] = y;
        }
        v_endif;
    }
    // [0] = 20.0F
    // [1] = -30.0F

    // NOT liveness across SETCC
    {
        vInt a = 0xFAAA;
        vInt b = 0x07BB;
        v_if (dst_reg[0] == 2.0F) {
            b = ~a;
        }
        v_endif;
        v_if (dst_reg[0] == 2.0F || dst_reg[0] == 3.0F) {
            v_if (dst_reg[0] == 2.0F) {
                set_expected_result(13, 40.0F, 0x0555, b);
            }
            v_endif;
            v_if (dst_reg[0] == 3.0F) {
                set_expected_result(13, 50.0F, 0x07BB, b);
            }
            v_endif;
        }
        v_endif;
    }
    // [2] = 40.0F
    // [3] = 50.0F

    // LZ liveness across SETCC
    {
        vInt a = 0x0080;
        vInt b = 0x07BB;
        v_if (dst_reg[0] == 4.0F) {
            b = lz(a);
        }
        v_endif;
        v_if (dst_reg[0] == 4.0F || dst_reg[0] == 5.0F) {
            v_if (dst_reg[0] == 4.0F) {
                set_expected_result(13, 60.0F, 11, b);
            }
            v_endif;
            v_if (dst_reg[0] == 5.0F) {
                set_expected_result(13, 70.0F, 0x07BB, b);
            }
            v_endif;
        }
        v_endif;
    }
    // [4] = 60.0F
    // [5] = 70.0F

    // MAD liveness across SETCC
    {
        vFloat a = 90.0F;
        vFloat b = 110.0F;
        v_if (dst_reg[0] == 6.0F) {
            b = a * a + 10.0;
        }
        v_endif;
        v_if (dst_reg[0] == 6.0F || dst_reg[0] == 7.0F) {
            dst_reg[13] = b;
        }
        v_endif;
    }
    // [6] = 8096.0
    // [7] = 110.0F

    // MOV liveness across SETCC
    {
        vFloat a = 120.0F;
        vFloat b = 130.0F;
        v_if (dst_reg[0] == 8.0F) {
            b = -a;
        }
        v_endif;
        v_if (dst_reg[0] == 8.0F || dst_reg[0] == 9.0F) {
            dst_reg[13] = b;
        }
        v_endif;
    }
    // [8] = -120.0F
    // [9] = 130.0F;

    // DIVP2 liveness across SETCC
    {
        vFloat a = 140.0F;
        vFloat b = 150.0F;
        v_if (dst_reg[0] == 10.0F) {
            b = addexp(a, 1);
        }
        v_endif;
        v_if (dst_reg[0] == 10.0F || dst_reg[0] == 11.0F) {
            dst_reg[13] = b;
        }
        v_endif;
    }
    // [10] = 280.0F
    // [11] = 150.0F

    // EXEXP liveness across SETCC
    {
        vFloat a = 160.0F;
        vInt b = 128;
        v_if (dst_reg[0] == 12.0F) {
            b = exexp_nodebias(a);
        }
        v_endif;
        v_if (dst_reg[0] == 12.0F || dst_reg[0] == 13.0F) {
            vFloat tmp = 1.0F;
            dst_reg[13] = setexp(tmp, b);
        }
        v_endif;
    }
    // [12] = 128.0F
    // [13] = 2.0F

    // EXMAN liveness across SETCC
    {
        vFloat a = 160.0F;
        vInt b = 128;
        v_if (dst_reg[0] == 14.0F) {
            b = exman8(a);
        }
        v_endif;
        v_if (dst_reg[0] == 14.0F || dst_reg[0] == 15.0F) {
            vFloat tmp = 128.0F;
            b = b << 9;
            dst_reg[13] = setman(tmp, b);
        }
        v_endif;
    }
    // [14] = 160.0F
    // [15] = 144.0F

    // SETEXP_I liveness across SETCC
    {
        vFloat a = 170.0F;
        vFloat b = 180.0F;
        v_if (dst_reg[0] == 16.0F) {
            b = setexp(a, 132);
        }
        v_endif;
        v_if (dst_reg[0] == 16.0F || dst_reg[0] == 17.0F) {
            dst_reg[13] = b;
        }
        v_endif;
    }
    // [16] = 42.5F
    // [17] = 180.0F

    // SETMAN_I liveness across SETCC
    {
        vFloat a = 190.0F;
        vFloat b = 200.0F;
        v_if (dst_reg[0] == 18.0F) {
            b = setman(a, 0x3AB);
        }
        v_endif;
        v_if (dst_reg[0] == 18.0F || dst_reg[0] == 19.0F) {
            dst_reg[13] = b;
        }
        v_endif;
    }
    // [18] = 245.0F
    // [19] = 200.0F

    // SETSGN_I liveness across SETCC
    {
        vFloat a = 210.0F;
        vFloat b = 220.0F;
        v_if (dst_reg[0] == 20.0F) {
            b = setsgn(a, 1);
        }
        v_endif;
        v_if (dst_reg[0] == 20.0F || dst_reg[0] == 21.0F) {
            dst_reg[13] = b;
        }
        v_endif;
    }
    // [20] = -210.0F
    // [21] = 220.0F

    // nonimm_dst_src using DIVP2 liveness across SETCC
    {
        vFloat a = 140.0F;
        vFloat b = 150.0F;
        v_if (dst_reg[0] == 22.0F) {
            b = addexp(a, imm - 34);
        }
        v_endif;
        v_if (dst_reg[0] == 22.0F || dst_reg[0] == 23.0F) {
            dst_reg[13] = b;
        }
        v_endif;
    }
    // [22] = 280.0F
    // [23] = 150.0F

    // nonimm_dst using LOADI liveness across SETCC
    {
        vFloat b = 240.0F;
        v_if (dst_reg[0] == 24.0F) {
            b = static_cast<float>(-imm);
        }
        v_endif;
        v_if (dst_reg[0] == 24.0F || dst_reg[0] == 25.0F) {
            dst_reg[13] = b;
        }
        v_endif;
    }
    // [24] = -35.0F
    // [25] = 240.0F

    copy_result_to_dreg0(13);
}

sfpi_test_noinline void test14(int imm)
{
    // Test13 tests various builtins for liveness across a SETCC
    // Below test MOV liveness across COMPC, LZ, EXEXP, IADD

    dst_reg[14] = -dst_reg[0];

    // MOV liveness across COMPC
    {
        vFloat a = 250.0F;
        vFloat b = 260.0F;
        v_if (dst_reg[0] != 0.0F) {
            b = 160.0F;
        } v_else {
            vFloat c = vConst0 * vConst0 + vConst0;
            b = -a;
            a = c;
        }
        v_endif;
        v_if (dst_reg[0] == 0.0F || dst_reg[0] == 1.0F) {
            dst_reg[14] = b;
        }
        v_endif;
    }
    // [0] = -250.0F
    // [1] = 160.0F;

    // MOV liveness across LZ
    {
        vFloat a = 250.0F;
        vFloat b = 260.0F;
        vInt tmp;

        v_if (dst_reg[0] == 2.0F) {
            v_if ((tmp = lz(a)) != 0) {
                vFloat c = vConst0 * vConst0 + vConst0;
                b = -a;
                a = c;
            }
            v_endif;
        }
        v_endif;
        v_if (dst_reg[0] == 2.0F || dst_reg[0] == 3.0F) {
            dst_reg[14] = b;
        }
        v_endif;
    }
    // [2] = -250.0F;
    // [3] = 260.0F

    // MOV liveness across EXEXP
    {
        vFloat a = 270.0F;
        vFloat b = 280.0F;
        vInt tmp;

        v_if (dst_reg[0] == 4.0F) {
            v_if ((tmp = exexp(a)) >= 0) {
                vFloat c = vConst0 * vConst0 + vConst0;
                b = -a;
                a = c;
            }
            v_endif;
        }
        v_endif;
        v_if (dst_reg[0] == 4.0F || dst_reg[0] == 5.0F) {
            dst_reg[14] = b;
        }
        v_endif;
    }
    // [4] = -270.0F;
    // [5] = 280.0F

    // Below 2 tests are incidentally covered by tests 1..12
    // MOV liveness across IADD
    {
        vFloat b = 300.0F;
        vInt tmp = 5;

        v_if (dst_reg[0] == 6.0F) {
            vFloat a = 290.0F;
            v_if (tmp >= 2) {
                vFloat c = vConst0 * vConst0 + vConst0;
                b = -a;
                a = c;
            }
            v_endif;
        }
        v_endif;
        v_if (dst_reg[0] == 6.0F || dst_reg[0] == 7.0F) {
            dst_reg[14] = b;
        }
        v_endif;
    }
    // [6] = -290.0F
    // [7] = 300.0F

    // IADD_I liveness
    {
        vInt a = 10;
        vInt b = 20;
        v_if (dst_reg[0] == 8.0F) {
            b = a + 30;
        }
        v_endif;
        v_if (dst_reg[0] == 8.0F || dst_reg[0] == 9.0F) {
            v_if (dst_reg[0] == 8.0F) {
                set_expected_result(14, -310.0F, 40, b);
            }
            v_endif;
            v_if (dst_reg[0] == 9.0F) {
                set_expected_result(14, 320.0F, 20, b);
            }
            v_endif;
        }
        v_endif;
    }
    // [8] = -310.0F
    // [9] = 320.0F

    // Test various issues with move/assign. Unfortunately, compiler generated
    // moves are hard/impossible to induce and not all scenarios are testable
    // w/ explicit code afaict.  The case #s below come from the Predicated
    // Variable Liveness document and similar code exists in live.cc

    // Case 2a
    // Assignment resulting in register rename
    {
        vFloat a = -20.0f;
        vFloat b = 30.0f;
        v_if (dst_reg[0] == 10.0f) {
            b = a;
        }
        v_endif;

        v_if (dst_reg[0] == 10.0F || dst_reg[0] == 11.0F) {
            dst_reg[14] = b;
        }
        v_endif;
    }
    // [10] = -20.0
    // [11] = 30.0

    // Case 2b
    // Assignment requiring move
    // This straddles case 2a and 3 - both values need to be preserved but the
    // compiler doesn't know that, solving case2a will solve this case as well
    {
        vFloat a = -40.0f;
        vFloat b = 50.0f;
        v_if (dst_reg[0] == 12.0f) {
            b = a;
        }
        v_endif;

        v_if (dst_reg[0] == 100.0f) { // always fail
            dst_reg[14] = a;
        }
        v_endif;

        v_if (dst_reg[0] == 12.0F || dst_reg[0] == 13.0F) {
            dst_reg[14] = b;
        }
        v_endif;
    }
    // [12] = -40.0
    // [13] = 50.0

    // Case 3
    // Assignment requiring move (both a and b need to be preserved)
    {
        vFloat a = -60.0f;
        vFloat b = 70.0f;
        v_if (dst_reg[0] == 14.0f) {
            b = a;
        }
        v_endif;

        v_if (dst_reg[0] == 100.0f) { // always fail
            dst_reg[14] = a + 1.0f;
        }
        v_endif;

        v_if (dst_reg[0] == 14.0F || dst_reg[0] == 15.0F) {
            dst_reg[14] = b + 1.0f;
        }
        v_endif;
    }
    // [14] = -59.0
    // [15] = 71.0

    // Case 4a
    // Destination as source, 2 arguments in the wrong order
    // Confirm b is correct
    {
        vInt a = 10;
        vInt b = 20;
        v_if (dst_reg[0] == 16.0f) {
            b = b - a;
        }
        v_endif;

        v_if (dst_reg[0] == 100.0f) { // always fail
            dst_reg[14] = a;
        }
        v_endif;

        v_if (dst_reg[0] == 16.0F) {
            set_expected_result(14, -80.0F, 10, b);
        }
        v_endif;

        v_if (dst_reg[0] == 17.0F) {
            set_expected_result(14, 90.0F, 20, b);
        }
        v_endif;
    }
    // [16] = -80.0
    // [17] = 90.0

    // Case 4b
    // Destination as source, 2 arguments in the wrong order
    // Confirm a is correct
    {
        vInt a = 10;
        vInt b = 20;
        v_if (dst_reg[0] == 16.0f) {
            b = b - a;
        }
        v_endif;

        v_if (dst_reg[0] == 100.0f) { // always fail
            dst_reg[14] = b;
        }
        v_endif;

        v_if (dst_reg[0] == 18.0F) {
            set_expected_result(14, -90.0F, 10, a);
        }
        v_endif;

        v_if (dst_reg[0] == 19.0F) {
            set_expected_result(14, 100.0F, 10, a);
        }
        v_endif;
    }
    // [18] = -90.0
    // [19] = 100.0

    // Case 4c
    // Destination as source 3 arguments
    // Confirm c is correct
    {
        // Out of regs doing this the typical way
        vFloat condition = dst_reg[0] - 20.0F;
        vInt a = 10;
        vInt b = 20;
        vInt c = 30;

        v_if (condition == 0.0F) {
            c = a - b;
        }
        v_endif;

        v_if (vConst0p8369 == dst_reg[0]) { // always fail
            dst_reg[14] = a;
            dst_reg[14] = b;
        }
        v_endif;

        v_if (dst_reg[0] == 20.0F) {
            set_expected_result(14, -100.0F, -10, c);
        }
        v_endif;

        v_if (dst_reg[0] == 21.0F) {
            set_expected_result(14, 110.0F, 30, c);
        }
        v_endif;
    }
    // [20] = -100.0
    // [21] = 110.0

    // Case 4c
    // Destination as source 3 arguments
    // Confirm a is correct
    {
        // Out of regs doing this the typical way
        vFloat condition = dst_reg[0] - 22.0F;
        vInt a = 10;
        vInt b = 20;
        vInt c = 30;

        v_if (condition == 0.0F) {
            c = a - b;
        }
        v_endif;

        v_if (vConst0p8369 == dst_reg[0]) { // always fail
            dst_reg[14] = a;
            dst_reg[14] = c;
        }
        v_endif;

        v_if (dst_reg[0] == 22.0F) {
            set_expected_result(14, -110.0F, 10, a);
        }
        v_endif;

        v_if (dst_reg[0] == 23.0F) {
            set_expected_result(14, 120.0F, 10, a);
        }
        v_endif;
    }
    // [22] = -110.0
    // [23] = 120.0

    // Case 4c
    // Destination as source 3 arguments
    // Confirm b is correct
    {
        // Out of regs doing this the typical way
        vFloat condition = dst_reg[0] - 24.0F;
        vInt a = 10;
        vInt b = 20;
        vInt c = 30;

        v_if (condition == 0.0F) {
            c = a - b;
        }
        v_endif;

        v_if (vConst0p8369 == dst_reg[0]) { // always fail
            dst_reg[14] = c;
            dst_reg[14] = b;
        }
        v_endif;

        v_if (dst_reg[0] == 24.0F) {
            set_expected_result(14, -120.0F, 20, b);
        }
        v_endif;

        v_if (dst_reg[0] == 25.0F) {
            set_expected_result(14, 130.0F, 20, b);
        }
        v_endif;
    }
    // [24] = -120.0
    // [25] = 130.0

    // The code below tests the case where we descend down a CC cascade, pop
    // back up, then back down w/ different CC bits set.  Does the variable
    // stay live when assigned at the same CC level but in a different
    // cascade, ie, across generations?
    {
        vFloat a;
        vFloat b;
        vFloat dr = dst_reg[0];

        v_if (dr == 26.0F || dr == 27.0F) {
            b = -90.0F;
        }
        v_endif;

        v_if (dr == 26.0F) {
            a = 100.0F;
        }
        v_endif;

        v_if (dr == 27.0F) {
            a = 110.0F;
        }
        v_endif;

        v_if (dr == 27.0F) {
            b = a;
        }
        v_endif;

        v_if (dr == 26.0F || dr == 27.0F) {
            dst_reg[14] = b;
        }
        v_endif;

        v_if (dr == 500.0F) {
            dst_reg[14] = a;
        }
        v_endif;
    }
    // [26] = -90.0F
    // [27] = 110.0F;

    // Test a little basic block liveness madness
    // NOTE: the test below hit a riscv gcc compiler bug where the float
    // library conversions were wrong where:
    //    (30.0f - i) != static_cast<float>(30 - i)
    // and not just due to rounding (off by orders of magnitude)
    {
        vFloat a = 200.0F;
        vFloat b = 1.0F;

        // unroll forces the compiler into multiple basic blocks
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC unroll 0
#endif
        for (int i = 0; i < imm - 30; i++) { // 0..4
            v_if (dst_reg[0] == 28.0F) {
                switch (i) {
                case 0:
                    b = 2.0f;
                    break;
                case 1:
                    b = 4.0f;
                    break;
                case 2:
                    b = 8.0f;
                    break;
                default:
                    b = b * 4.0F;
                }
            } v_elseif (dst_reg[0] >= static_cast<float>(30 - i)) {
                if (i % 2 == 0) {
                    b = 10.0F;
                } else {
                    b = 20.0F;
                }
            }
            v_endif;

            a = a + a * b;
        }

        v_if (dst_reg[0] == 28.0F || dst_reg[0] == 29.0F) {
            dst_reg[14] = a;
        }
        v_endif;
    }
    // [28] = 200+200*2, 600+600*4, 3000+3000*8, 27000+27000*32, 89100+89100*128 =
    //        114939000.0F or 114819072.0F when rounded
    // [29] = 200+200*1, 400+400*20, 4400+4400*20, 92400+92400*10, 1016400+1016400*20 =
    //        21344400.0F or 21233664.0F when rounded

    copy_result_to_dreg0(14);
}

//////////////////////////////////////////////////////////////////////////////
// These tests are designed to be incremental so that if a test fails the
// earlier tests should be examined/fixed prior to the latter tests.
//
template <SfpiTestType operation>
inline void calculate_sfpi(uint param0 = 0, uint param1 = 0, uint param2 = 0, uint param3 = 0, uint param4 = 0, uint param5 = 0)
{
    if constexpr (operation == SfpiTestType::test1) {
        test1();
    } else if constexpr (operation == SfpiTestType::test2) {
        test2();
    } else if constexpr (operation == SfpiTestType::test3) {
        test3();
    } else if constexpr (operation == SfpiTestType::test4) {
        test4();
    } else if constexpr (operation == SfpiTestType::test5) {
        test5();
    } else if constexpr (operation == SfpiTestType::test6) {
        test6();
    } else if constexpr (operation == SfpiTestType::test7) {
        test7();
    } else if constexpr (operation == SfpiTestType::test8) {
        test8();
    } else if constexpr (operation == SfpiTestType::test9) {
        test9();
    } else if constexpr (operation == SfpiTestType::test10) {
        test10();
    } else if constexpr (operation == SfpiTestType::test11) {
        test11();
    } else if constexpr (operation == SfpiTestType::test12) {
        test12(param0);
    } else if constexpr (operation == SfpiTestType::test13) {
        test13(param0);
    } else if constexpr (operation == SfpiTestType::test14) {
        test14(param0);
    }
}

} // NAMESPACE
