/*
 * c_fw_defs.h
 *
 * Common definitions for C-based F/W
 */

#ifndef _C_FW_DEFS_H_
#define _C_FW_DEFS_H_

#include <stdint.h>

#define __PACKING_ATTRIBUTE_FIELD_LEVEL__
#define __PACKING_ATTRIBUTE_STRUCT_END__
#define INLINE static inline

#undef FIRMWARE_LITTLE_ENDIAN

#define CONST_0 0

register unsigned int R0 asm("r0");
register unsigned int R1 asm("r1");
register unsigned int R2 asm("r2");
register unsigned int R3 asm("r3");
register unsigned int R4 asm("r4");
register unsigned int R5 asm("r5");
register unsigned int R6 asm("r6");
register unsigned int R7 asm("r7");

register unsigned int r18 asm("r18");
register unsigned int R18 asm("r18");
register unsigned int r19 asm("r19");
register unsigned int R19 asm("r19");
register unsigned int r20 asm("r20");
register unsigned int R20 asm("r20");
register unsigned int r31 asm("r31");
register unsigned int R31 asm("r31");

#if __has_builtin(__builtin_expect)
#define likely(_expr)           __builtin_expect((_expr) != 0, 1)
#define unlikely(_expr)         __builtin_expect((_expr) != 0, 0)
#else
#define likely(_expr)           (_expr)
#define unlikely(_expr)         (_expr)
#endif

typedef uint64_t * aligned_uint64_ptr __attribute__((align_value(8)));
typedef uint32_t * aligned_uint32_ptr __attribute__((align_value(8)));
typedef uint16_t * aligned_uint16_ptr __attribute__((align_value(8)));
typedef uint8_t * aligned_uint8_ptr __attribute__((align_value(8)));

#define PSRAM_NEXT_REQ_NUM      1

#define STRINGIFY(x)            # x
#define CONCAT(a, b)            STRINGIFY(a##b)
#define UNIQUENAME(prefix)      CONCAT(prefix, __LINE__)

/* Get field given the field name.
 * <field_name>_F_OFFSET and <field_name>_F_WIDTH must be defined
 */
#define FIELD_GET(_result, _word, _fld)  \
    _result = extract((uint32_t)(_word), _fld ## _F_OFFSET, _fld ## _F_WIDTH)

/* Get field given the field offset and width */
#define FIELD_GET_OW(_result, _word, _o, _w)  \
    _result = extract((uint32_t)(_word), _o, _w)

/* Set field given the field name.
 * <field_name>_F_OFFSET and <field_name>_F_WIDTH must be defined
 */
#define FIELD_SET(_result, _fld, _val) \
    _result = (typeof(_result))insert((uint32_t)(_result), (uint32_t)(_val), _fld ## _F_OFFSET, _fld ## _F_WIDTH, 0)

/* Set field given the field offset, length and optional source offset */
#define FIELD_SET_OW(_result, _val, _o, _w, _so) \
    _result = (typeof(_result))insert((uint32_t)(_result), (uint32_t)(_val), _o, _w, _so)

/* Clear field given the field name.
 * <field_name>_F_OFFSET and <field_name>_F_WIDTH must be defined
 */
#define FIELD_CLEAR(_result, _fld) \
    _result = (typeof(_result))insert((uint32_t)(_result), r0, _fld ## _F_OFFSET, _fld ## _F_WIDTH, 0)

/* Clear field given the field offset and length */
#define FIELD_CLEAR_OW(_result, _o, _w) \
    _result = (typeof(_result))insert((uint32_t)(_result), r0, _o, _w, 0)

/* Copy from 1 field to another */
#define FIELD_COPY(_result, _fld, _val, _from_fld) \
    _result = (typeof(_result))insert((uint32_t)(_result), (uint32_t)(_val), _fld ## _F_OFFSET, _fld ## _F_WIDTH, _from_fld ## _F_OFFSET)

/* STIO8 */
#define STIO8(_val, _io_address) \
    __asm__ ("stio8 %0 " # _io_address \
        : : "r"(_val))

/* STIO32 */
#define STIO32(_val, _io_address) \
    __asm__ ("stio32 %0 " # _io_address \
        : : "r"(_val))
          
          
/* NOP */
#define NOP() \
    do { \
        __asm__ __volatile__("nop\n");\
    } while (0)

/* CTX_SWAP */
#define CTX_SWAP(_label, _options) \
    do { \
        __asm__ __volatile__(\
            "// ctx_swap: _label _options\n" \
            "    ctx_swap  :" # _label " " # _options );   \
        NOP(); \
        NOP(); \
    } while (0)

/* PCKSUM */
#define PCHKSM(_src_addr, _len, _res_addr, _options) \
    do { \
        __asm__ __volatile__(\
            "// pchksm parameters: sram_address, length, result_address\n" \
            "    pchksm  %0  %1  %2 " # _options \
              : : "r"(_src_addr), "r"(_len), "r"(_res_addr)); \
    } while (0)

/* PCKSUM with "ctx_swap" options */
#define PCHKSM_WAIT(_src_addr, _len, _res_addr, _options) \
    do { \
        PCHKSM(_src_addr, _len, _res_addr, _options invoke ctx_swap); \
        NOP(); \
        NOP(); \
    } while (0)

/* CRC32
 * _area_ptr = (result_address << 16) | src_addr
 * _length - area length
 * _init_val - initial accumulator values
 */
#define CRC32(_area_ptr, _length, _init_val, _options) \
    do { \
        __asm__ __volatile__(\
            "crc32  %0  %1  %2  " # _options \
              : : "r"(_area_ptr), "r"(_length), "r"(_init_val)); \
    } while (0)

/* CRC32 with ctx_swap option */
#define CRC32_WAIT(_area_ptr, _length, _init_val, _options) \
    do { \
        CRC32(_area_ptr, _length, _init_val, _options invoke ctx_swap); \
        NOP(); \
        NOP(); \
    } while (0)

/* Invoke checksum instruction */
#define CHKSM(_checksum, _old, _new, _options)\
    do { \
        __asm__ __volatile__(\
            "chksm  %0  %1  %2 " # _options "\n"\
            "    nop\n"\
            "    chksm  %0  R0  R0  last"\
              : "+r"(_checksum) : "r"(_old), "r"(_new)); \
    } while (0)

/* Update IP/TCP/UDP checksum using pre-calculated delta */
#define CHKSM_UPDATE(_checksum, _delta)\
    do { \
        __asm__ __volatile__(\
            "chksm  %0  R0  %1  low\n" \
            "    nop\n"\
            "    chksm  %0  R0  R0  last"\
              : "+r"(_checksum) : "r"(_delta)); \
    } while (0)


/* Dummy store. Needed in RDD4G to avoid ld after st restriction */
#define DUMMY_STORE() \
    do { \
        __asm__ __volatile__( \
            "START_DONT_OPTIMIZE( )\n " \
            "st8  R0  SRAM_DUMMY_STORE_ADDRESS\n" \
            "nop\n" \
            "nop\n" \
            "END_OPTIMIZE_HINT( )" ); \
    } while (0)

INLINE void debug_nop(int num_of_nops)
{
    int i;

    __asm__ __volatile__("START_DONT_OPTIMIZE( )\n");
    for (i = 0; i < num_of_nops; i++)
        NOP();
    __asm__ __volatile__("END_OPTIMIZE_HINT( )\n");
}

#if 0
/* Get next SBPM segment */
#define SBPM_GET_NEXT(_buffer_number, _scratch, _task) \
    do { \
        uint64_t _msg; \
        uint32_t *_msg2 = (uint32_t *)&_msg; \
        FIELD_CLEAR(_buffer_number, DMA_COMMAND_DATA_SOP); \
        _msg2[0] =  (_task << (SBPM_GET_NEXT_REQUEST_TASK_NUM_F_OFFSET_MOD16 + 16)) | \
                    ((_scratch >> 3) << (SBPM_GET_NEXT_REQUEST_TARGET_ADDRESS_F_OFFSET)) | \
                    (1 << (SBPM_GET_NEXT_REQUEST_WAKEUP_F_OFFSET)); \
        _msg2[1] = _buffer_number | (PSRAM_NEXT_REQ_NUM << SBPM_MULTI_GET_NEXT_REQUEST_NUM_OF_BUFFERS_F_OFFSET); \
        bbmsg64(SBPM_OPCODE_MULTI_GET_NEXT, BB_ID_SBPM, _msg);\
        ctx_swap(OPT_NO_ASYNCH_ENABLE); \
        _buffer_number = *(uint16_t *)_scratch; \
    } while (0)

#else

/* Get next SBPM segment */
#define SBPM_GET_NEXT(_buffer_number, _scratch, _task) \
    do { \
        __asm__ __volatile__(\
            "DECLARE_DOUBLE_REG_VAR(_sbpm_msg)\n"\
            "    DECLARE_FULL_REG_VAR(_bb_dest)\n"\
            "    mov     _bb_dest  BB_ID_SBPM  clear\n"\
            "    insert  %0->DMA_COMMAND_DATA_SOP  R0\n"\
            "    alu     _sbpm_msg_1  %0  OR  PSRAM_NEXT_REQ_NUM <<SBPM_MULTI_GET_NEXT_REQUEST_NUM_OF_BUFFERS_F_OFFSET \n"\
            "    mov     _sbpm_msg_0  (" # _task " << SBPM_GET_NEXT_REQUEST_TASK_NUM_F_OFFSET_MOD16)  <<16  clear \n"\
            "    insert  _sbpm_msg_0  %1  SBPM_GET_NEXT_REQUEST_TARGET_ADDRESS_F_OFFSET  SBPM_GET_NEXT_REQUEST_TARGET_ADDRESS_F_WIDTH  d.3 \n"\
            "    insert  _sbpm_msg_0  CONST_1  SBPM_GET_NEXT_REQUEST_WAKEUP_F_OFFSET  SBPM_GET_NEXT_REQUEST_WAKEUP_F_WIDTH \n"\
            "    ctx_swap " UNIQUENAME(sbpm_get_next_done_) "\n"\
            "    bbmsg   SBPM_OPCODE_MULTI_GET_NEXT  _bb_dest  _sbpm_msg_0  64bit \n"\
            "    nop \n"\
            "    :" UNIQUENAME(sbpm_get_next_done_) "\n"\
            "    ld16    %0  %1  SBPM_GET_NEXT_REPLY_NEXT_BN_OFFSET \n"\
            "    extract %0  %0  SBPM_MULTI_GET_NEXT_REPLY_BN1_F_OFFSET  SBPM_MULTI_GET_NEXT_REPLY_BN1_F_WIDTH "\
            : "+r"(_buffer_number) : "r"(_scratch));\
    } while (0)
#endif

/*
 * Counter
 */
/* Increment counter */
#if 0
#define CTR_INC(_ctr_id, _value, _group) \
    do { \
        __asm__ __volatile__(\
            "    ctr  %0  CONST_1  %1  " # _group \
              : : "r"(_ctr_id), "r"(_value)); \
    } while (0)
#endif

#if 1
#define CTR_INC(_ctr_id, _value, _group) \
	counter(_ctr_id, CONST_1, _value, _group, OPT_COUNTER_INC, OPT_COUNTER_INC, OPT_NO_INVOKE, OPT_NO_CTX_SWAP )
#endif

/*
 * Macros for mixing C and assembler
 */

#define C_DECLARE_TASK(_label) \
    __asm__ __volatile__("DECLARE_TASK(" # _label ")")

#define C_MARK_TAG(_tag) \
    __asm__ __volatile__("MARK_TAG(" # _tag ")")

#define C_DEFINE_TAG(_tag) \
    __asm__ __volatile__("DEFINE_TAG(" # _tag ")")

#define C_BRANCH_TO_TAG(_tag) \
    __asm__ __volatile__("BRANCH_TO_TAG(" # _tag ")")

#define C_MARK_JUMP_AS_FUNCTION_RETURN() \
    __asm__ __volatile__("MARK_JUMP_AS_FUNCTION_RETURN()")

#define C_MARK_JUMP_AS_FUNCTION_CALL() \
    __asm__ __volatile__("MARK_JUMP_AS_FUNCTION_CALL()")

#define C_MARK_JUMP_AS_FUNCTION_CALL_WITH_RETURN_TARGET(_tag) \
    __asm__ __volatile__("MARK_JUMP_AS_FUNCTION_CALL_WITH_RETURN_TARGET(" #_tag ")")

#define JMP_LABEL(_label) \
    __asm__ __volatile__("ljmp  :" # _label)

#define JMP_LABEL_P1(_label, _p1) \
    do { \
        __asm__ __volatile__("ljmp  :" #_label);\
        __asm__ __volatile__("alu  " #_label "_r2 R0 + %0" : : "r"(_p1));\
        NOP(); \
    } while (0)

#define JMP_LABEL_P2(_label, _p1, _p2) \
    do { \
        __asm__ __volatile__("ljmp  :" #_label);\
        __asm__ __volatile__("alu  " #_label "_r2 R0 + %0" : : "r"(_p1));\
        __asm__ __volatile__("alu  " #_label "_r3 R0 + %0" : : "r"(_p2));\
    } while (0)

#define JMP(_addr, _options) \
    __asm__ __volatile__("jmp  " # _addr " " # _options)


#include "rdd_data_structures_auto.h"
#include "fw_defs_auto.h"
#include "fw_defs.h"
#include "fw_runner_defs_auto.h"
#include "fw_runner_defs.h"
#include "runner_intrinsics.h"
#include "rdd_runner_proj_defs.h"

#endif /* _C_FW_DEFS_H_ */
