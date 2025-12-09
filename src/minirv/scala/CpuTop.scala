package minirv.scala

import chisel3._
import chisel3.util.{Cat, Decoupled}
import minirv.scala.core._
import minirv.scala.common._

class RegFile extends Module {
    val io = IO(new Bundle {
        val RF_raddr1 = Input(UInt(5.W))
        val RF_raddr2 = Input(UInt(5.W))
        val RF_rdata1 = Output(UInt(32.W))
        val RF_rdata2 = Output(UInt(32.W))
        val RF_wen = Input(Bool())
        val RF_waddr = Input(UInt(5.W))
        val RF_wdata = Input(UInt(32.W))
        val debug_r10 = Output(UInt(32.W))
        val RF_value = Output(Vec(32, UInt(32.W)))
    })
    val reg_file = RegInit(VecInit(Seq.fill(32)(0.U(32.W))))
    io.RF_rdata1 := reg_file(io.RF_raddr1)
    io.RF_rdata2 := reg_file(io.RF_raddr2)
    io.debug_r10 := reg_file(10)
    io.RF_value := reg_file
    when (io.RF_wen && io.RF_waddr =/= 0.U) {
        reg_file(io.RF_waddr) := io.RF_wdata
    }
}

class CpuTop extends Module {
    val io = IO(new Bundle{
        val debug_pc = Output(UInt(32.W))
        val debug_rf_wen = Output(Bool())
        val debug_rf_waddr = Output(UInt(5.W))
        val debug_rf_wdata = Output(UInt(32.W))
        val ebreak = Output(Bool())
        val debug_r10 = Output(UInt(32.W))
    })

    val reg_file = Module(new RegFile)
    io.debug_r10 := reg_file.io.debug_r10
    val register_dpi = Module(new RegisterDPI)
    register_dpi.io.clock := clock
    register_dpi.io.reset := reset
    register_dpi.io.RF_value := reg_file.io.RF_value

    val ifu = Module(new IFU)
    val idu = Module(new IDU)
    val exu = Module(new EXU)
    val lsu = Module(new LSU)
    val wbu = Module(new WBU)

    val inst_memory = Memory_DPI.createMemoryInterface()
    val data_memory = Memory_DPI.createMemoryInterface()

    inst_memory.io.req <> ifu.io.memReq
    ifu.io.br_taken := idu.io.br_taken
    ifu.io.br_target := idu.io.br_target
    ifu.io.ebreak := idu.io.out.bits.ebreak

    idu.io.in <> ifu.io.out
    idu.io.memResp <> inst_memory.io.resp
    reg_file.io.RF_raddr1 := idu.io.rfRaddr1
    reg_file.io.RF_raddr2 := idu.io.rfRaddr2
    idu.io.rfRdata1 := reg_file.io.RF_rdata1
    idu.io.rfRdata2 := reg_file.io.RF_rdata2
    idu.io.alu_result := exu.io.alu_result
    idu.io.LS_valid := lsu.io.in.valid
    idu.io.LS_rfWen := lsu.io.in.bits.rfWen
    idu.io.LS_rfWaddr := lsu.io.in.bits.rfWaddr
    idu.io.LS_res_from_mem := lsu.io.in.bits.res_from_mem
    idu.io.LS_alu_result := lsu.io.in.bits.alu_result
    idu.io.WB_valid := wbu.io.in.valid
    idu.io.WB_rfWen := wbu.io.in.bits.rfWen
    idu.io.WB_rfWaddr := wbu.io.in.bits.rfWaddr
    idu.io.WB_rfWdata := wbu.io.rf_wdata
    
    exu.io.in <> idu.io.out

    lsu.io.in <> exu.io.out
    lsu.io.memReq <> data_memory.io.req

    wbu.io.in <> lsu.io.out
    wbu.io.memResp <> data_memory.io.resp
    reg_file.io.RF_wen := wbu.io.rf_wen
    reg_file.io.RF_waddr := wbu.io.rf_waddr
    reg_file.io.RF_wdata := wbu.io.rf_wdata

    io.debug_pc := wbu.io.pc
    io.debug_rf_wen := wbu.io.rf_wen
    io.debug_rf_waddr := wbu.io.rf_waddr
    io.debug_rf_wdata := wbu.io.rf_wdata
    io.ebreak := wbu.io.ebreak
}
