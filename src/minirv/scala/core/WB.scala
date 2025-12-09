package minirv.scala.core
import chisel3._
import chisel3.util.{Decoupled, MuxLookup}
import minirv.scala.common._
import os.read

class WBU extends Module{
    val io = IO(new Bundle{
        val in = Flipped(Decoupled(new LS2WB))
        val memResp = Flipped(Decoupled(new MemResp))
        val pc = Output(UInt(32.W))
        val rf_wen = Output(Bool())
        val rf_waddr = Output(UInt(5.W))
        val rf_wdata = Output(UInt(32.W))
        val ebreak = Output(Bool())
    })

    val ready_go = !io.in.valid || io.memResp.fire

    val mem_result = MuxLookup(io.in.bits.mem_ctrl(1, 0), 0.U(32.W))(
        Seq(
            "b00".U(2.W) -> MuxLookup(io.in.bits.alu_result(1, 0), 0.U(8.W))(Seq(
                "b00".U(2.W) -> BitUtils.zext(io.memResp.bits.rdata(7, 0), 8),
                "b01".U(2.W) -> BitUtils.zext(io.memResp.bits.rdata(15, 8), 8),
                "b10".U(2.W) -> BitUtils.zext(io.memResp.bits.rdata(23, 16), 8),
                "b11".U(2.W) -> BitUtils.zext(io.memResp.bits.rdata(31, 24), 8)
            )),
            "b01".U(2.W) -> MuxLookup(io.in.bits.alu_result(1, 0), 0.U(16.W))(Seq(
                "b00".U(2.W) -> BitUtils.zext(io.memResp.bits.rdata(15, 0), 16),
                "b10".U(2.W) -> BitUtils.zext(io.memResp.bits.rdata(31, 16), 16)
            )),
            "b10".U(2.W) -> io.memResp.bits.rdata
        )
    )
    io.memResp.ready := 1.B
    val final_result = Mux(io.in.bits.res_from_mem, mem_result, io.in.bits.alu_result)

    io.in.ready := ready_go
    io.pc := io.in.bits.pc
    io.rf_wen := io.in.bits.rfWen && io.in.valid
    io.rf_waddr := io.in.bits.rfWaddr
    io.rf_wdata := final_result
    io.ebreak := io.in.bits.ebreak && io.in.valid

    dontTouch(ready_go)
    dontTouch(mem_result)
    dontTouch(final_result)

    DebugSignals.tap(this, io.pc, "wbu.pc")
    DebugSignals.tap(this, io.rf_wen.asUInt, "wbu.rf_wen")
    DebugSignals.tap(this, io.rf_waddr, "wbu.rf_waddr")
    DebugSignals.tap(this, io.rf_wdata, "wbu.rf_wdata")
    DebugSignals.tap(this, ready_go.asUInt, "wbu.ready_go")
    DebugSignals.tap(this, mem_result, "wbu.mem_result")
    DebugSignals.tap(this, final_result, "wbu.final_result")
    DebugSignals.tap(this, io.in.valid.asUInt, "wbu.io.in.valid")
    DebugSignals.tap(this, io.in.ready.asUInt, "wbu.io.in.ready")
    DebugSignals.tap(this, io.in.bits.asUInt, "wbu.io.in.bits")
    DebugSignals.tap(this, io.memResp.valid.asUInt, "wbu.io.memResp.valid")
    DebugSignals.tap(this, io.memResp.ready.asUInt, "wbu.io.memResp.ready")
    DebugSignals.tap(this, io.memResp.bits.asUInt, "wbu.io.memResp.bits")
}
