// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "ckernel_template.h"
#include "cunpack_common.h"
#include "ckernel_globals.h"

using namespace ckernel;
using namespace ckernel::unpacker;

template <PoolType type, ReduceDim dim>
inline void _llk_unpack_reduce_mop_config_(const std::uint32_t num_faces = 4) {
#if SKIP_UNP == 1
    static constexpr uint unpack_srca = TT_OP_NOP;
#else
    static constexpr uint unpack_srca =
        TT_OP_UNPACR(SrcA, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
#endif
    static constexpr uint unpack_zerosrca = TT_OP_UNPACR_NOP(SrcA, p_unpacr::UNP_ZEROSRC);
#if SKIP_UNP == 1
    static constexpr uint unpack_srcb = TT_OP_NOP;
#else
    static constexpr uint unpack_srcb =
        TT_OP_UNPACR(SrcB, 0b0, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
#endif
    const uint32_t outerloop = num_faces;
    constexpr uint32_t innerloop = 1;
    ckernel_template tmp(outerloop, innerloop, unpack_zerosrca, unpack_srca);
    tmp.set_start_op(unpack_srcb);
    tmp.program(instrn_buffer);
}

template <PoolType type, ReduceDim dim>
inline void _llk_unpack_reduce_hw_configure_(
    const std::uint32_t unpack_src_format, const std::uint32_t unpack_dst_format) {
    configure_unpack_AB(
        unpack_src_format,
        unpack_src_format,
        unpack_dst_format,
        unpack_dst_format,
        16,16,true
    );
}

template <PoolType type, ReduceDim dim>
// within_face_16x16_transpose is used on WH but not used for GS, this transpose is done in math on GS
inline void _llk_unpack_reduce_init_(const std::uint32_t within_face_16x16_transpose=0, const std::uint32_t num_faces = 4) {
    _llk_unpack_reduce_mop_config_<type, dim>(num_faces);
}

template <PoolType type, ReduceDim dim>
inline void _llk_unpack_reduce_(const std::uint32_t address) {

    // Clear z/w start counters
    TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111);

    // Program srcA and srcB base addresses
    volatile uint tt_reg_ptr *cfg = get_cfg_pointer();  // get pointer to registers for current state ID

    // Wait for free context
    wait_for_next_context(2);

    // Load only 16 datums into srcB
    TTI_SETADCXX(p_setadc::UNP1, DATUMS_PER_ROW-1, 0x0);

    // Get tile address
    if (0 == unp_cfg_context) {
        cfg[THCON_SEC0_REG3_Base_address_ADDR32] = address;
    } else {
        cfg[THCON_SEC0_REG3_Base_cntx1_address_ADDR32] = address;
    }

    // Trisc::SEMPOST for context acquire
    semaphore_post(semaphore::UNPACK_SYNC);

    // Stall unpacker until pending CFG writes from Trisc have completed
    TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::TRISC_CFG);

    // Run MOP
    ckernel::ckernel_template::run(instrn_buffer);

    // Restore face height
    TTI_SETADCXX(p_setadc::UNP1, FACE_HEIGHT*16-1, 0x0);

    // T6::SEMGET for context release
    t6_semaphore_get(semaphore::UNPACK_SYNC);

    // Switch unpacker config context
    switch_config_context(unp_cfg_context);

#ifdef PERF_DUMP
    first_unpack_recorded = true;
#endif
}
