typedef unsigned int uint32;
typedef unsigned long long uint64;

// accelerators options

#define OPT_LINK                        1
#define OPT_NO_LINK                     0
#define OPT_INVOKE			1
#define OPT_NO_INVOKE 			0
#define OPT_CTX_SWAP			1
#define OPT_NO_CTX_SWAP			0
#define OPT_LAST			1
#define OPT_NO_LAST			0
#define OPT_UPDATE			1
#define OPT_NO_UPDATE			0
#define OPT_REPLY                       1
#define OPT_NO_REPLY                    0
#define OPT_ASYNCH_ENABLE		1
#define OPT_NO_ASYNCH_ENABLE	        0

#define OPT_NATC_SIZE_128		0
#define OPT_NATC_SIZE_256		1
#define OPT_NATC_SIZE_DEFAULT		OPT_NATC_SIZE_128

#define OPT_HASH_CFG_TBL_0		1
#define OPT_HASH_CFG_TBL_1		2
#define OPT_HASH_CFG_TBL_2		3
#define OPT_HASH_CFG_TBL_3		4
#define OPT_HASH_CFG_TBL_4		5
#define OPT_HASH_CFG_TBL_5		6
#define OPT_HASH_CFG_TBL_6		7
#define OPT_HASH_AGING			1
#define OPT_HASH_NO_AGING		0

#define OPT_COUNTER_INC         1
#define OPT_COUNTER_DEC         0
         
// FFI
extern  uint32 ffi8(uint32, uint32) asm("llvm.runner.ffi8");
extern  uint32 ffi32(uint32, uint32) asm("llvm.runner.ffi32");
extern  uint32 ffi16(uint32, uint32) asm("llvm.runner.ffi16");
extern  uint32 ffi8_reverse(uint32, uint32) asm("llvm.runner.ffi8.reverse");
extern  uint32 ffi16_reverse(uint32, uint32) asm("llvm.runner.ffi16.reverse");
extern  uint32 ffi32_reverse(uint32, uint32) asm("llvm.runner.ffi32.reverse");

// insert and extract
extern  uint32 insert(uint32 dst, uint32 src, uint32 dst_offset, uint32 width, uint32 src_offset) asm("llvm.runner.insert");
extern  uint32 extract(uint32 src, uint32 src_offset, uint32 width) asm("llvm.runner.extract");

// bytes-swap
extern  uint32 bytes_swap(uint32 src, uint32 byte_3, uint32 byte_2, uint32 byte_1, uint32 byte_0) asm("llvm.runner.bytes.swap");

/* Global register + R16, R17 access */
register unsigned int r0  asm("r0");
register unsigned int r1  asm("r1");
register unsigned int r2  asm("r2");
register unsigned int r3  asm("r3");
register unsigned int r4  asm("r4");
register unsigned int r5  asm("r5");
register unsigned int r6  asm("r6");
register unsigned int r7  asm("r7");
register unsigned int r16 asm("r16");
register unsigned int r17 asm("r17");

// Accelerators
extern  void dma_read(uint32 ddr_addr, uint32 sram_addr, uint32 byte_cnt, uint32 link, uint32 asynch_en, uint32 invoke, uint32 update, uint32 ctx_swap) asm("llvm.runner.dma.read");
extern  void dma_read_psram(uint32 ddr_addr, uint32 sram_addr, uint32 byte_cnt, uint32 link, uint32 asynch_en, uint32 invoke, uint32 update, uint32 ctx_swap) asm("llvm.runner.dma.read.psram");
extern  void dma_read_ddr(uint32 ddr_addr, uint32 sram_addr, uint32 byte_cnt, uint32 link, uint32 asynch_en, uint32 invoke, uint32 update, uint32 ctx_swap) asm("llvm.runner.dma.read.ddr");					     
extern  void dma_read_40bitaddr(uint64 ddr_addr, uint32 sram_addr, uint32 byte_cnt, uint32 link, uint32 asynch_en, uint32 invoke, uint32 update, uint32 ctx_swap) asm("llvm.runner.dma.read.40bit");
extern  void dma_write(uint32 ddr_addr, uint32 sram_addr, uint32 byte_cnt, uint32 reply, uint32 asynch_en, uint32 invoke, uint32 update, uint32 ctx_swap) asm("llvm.runner.dma.write");
extern  void dma_write_psram(uint32 ddr_addr, uint32 sram_addr, uint32 byte_cnt, uint32 reply, uint32 asynch_en, uint32 invoke, uint32 update, uint32 ctx_swap) asm("llvm.runner.dma.write.psram");
extern  void dma_write_ddr(uint32 ddr_addr, uint32 sram_addr, uint32 byte_cnt, uint32 reply, uint32 asynch_en, uint32 invoke, uint32 update, uint32 ctx_swap) asm("llvm.runner.dma.write.ddr");
extern  void dma_write_40bitaddr(uint64 ddr_addr, uint32 sram_addr, uint32 byte_cnt, uint32 reply, uint32 asynch_en, uint32 invoke, uint32 update, uint32 ctx_swap) asm("llvm.runner.dma.write.40bit");

extern  void parser(uint32 packet_addr, uint32 hlan, uint32 plen, uint32 result_addr, uint32 link, uint32 invoke, uint32 ctx_swap) asm("llvm.runner.parser");
extern  void pchksm(uint32 packet_addr, uint32 hlan, uint32 result_addr, uint32 link, uint32 invoke, uint32 ctx_swap) asm("llvm.runner.checksum");

extern  void hash(uint32 cmd_addr, uint32 reply_addr, uint32 cfg, uint32 age, uint32 invoke, uint32 ctx_swap) asm("llvm.runner.hash");
extern  void natc(uint32 cmd_addr, uint32 reply_addr, uint32 size, uint32 invoke, uint32 ctx_swap) asm("llvm.runner.natc");
extern  void counter(uint32 cntr_id, uint32 cntr0_size, uint32 cntr1_size, uint32 group_id, uint32 cntr0_action, uint32 cntr1_action, uint32 invoke, uint32 ctx_swap) asm("llvm.runner.ctr");
extern  void crc32(uint32, uint32, uint32, uint32, uint32, uint32, uint32, uint32) asm("llvm.runner.crc32");
extern  void load_io(uint32, uint32, uint32, uint32) asm("llvm.runner.ldio");
extern  void store_io(uint32, uint32, uint32, uint32) asm("llvm.runner.stio");
extern  void cam_lkp(uint32, uint32, uint32, uint32, uint32, uint32, uint32, uint32) asm("llvm.runner.cam_lkp");

extern  void bbmsg32(uint32 type, uint32 bb_dest, uint32 msg) asm("llvm.runner.bbmsg32");
extern  void bbmsg64(uint32 type, uint32 bb_dest, uint64 msg) asm("llvm.runner.bbmsg64");
extern  void bbmsg128(uint32 type, uint32 bb_dest, uint64 msg_part1, uint64 msg_part2) asm("llvm.runner.bbmsg128");

extern  void ctx_swap(uint32 asynch_en) asm("llvm.runner.ctx.swap");
