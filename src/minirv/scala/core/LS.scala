package minirv.scala.core
import chisel3._
import chisel3.util.{Decoupled, Queue, MuxLookup}
import minirv.scala.core.{EX2LS, LS2WB}
import minirv.scala.common._

class LSU extends Module{
    val io = IO(new Bundle{
        val in = Flipped(Decoupled(new EX2LS))
        val out = Decoupled(new LS2WB)
        val memReq = Decoupled(new MemReq)
    })

    val ready_go = !io.in.valid || io.memReq.fire

    val byte_off = io.in.bits.alu_result(1, 0)
    val baseStrobe: UInt = MuxLookup(io.in.bits.mem_ctrl(1, 0), 0.U(4.W))(
        Seq(
            "b00".U(2.W) -> ((1.U(4.W) << byte_off)(3, 0)),
            "b01".U(2.W) -> ((3.U(4.W) << byte_off)(3, 0)),
            "b10".U(2.W) -> "b1111".U(4.W)
        )
    )
    val wdata: UInt = MuxLookup(io.in.bits.mem_ctrl(1, 0), 0.U(32.W))(
        Seq(
            "b00".U(2.W) -> (io.in.bits.value2(7, 0) << (byte_off * 8.U)),
            "b01".U(2.W) -> (io.in.bits.value2(15, 0) << (byte_off * 8.U)),
            "b10".U(2.W) -> io.in.bits.value2
        )
    )

    io.memReq.valid := io.in.valid && io.out.ready
    io.memReq.bits.ren := io.in.bits.res_from_mem && io.in.valid
    io.memReq.bits.wen := io.in.bits.memWen && io.in.valid
    io.memReq.bits.addr := io.in.bits.alu_result & (~3.U(32.W))
    io.memReq.bits.wstrb := baseStrobe
    io.memReq.bits.wdata := wdata

    val to_enq = Wire(new LS2WB)
    to_enq.pc := io.in.bits.pc
    to_enq.mem_ctrl := io.in.bits.mem_ctrl
    to_enq.alu_result := io.in.bits.alu_result
    to_enq.res_from_mem := io.in.bits.res_from_mem
    to_enq.rfWen := io.in.bits.rfWen
    to_enq.rfWaddr := io.in.bits.rfWaddr
    to_enq.ebreak := io.in.bits.ebreak

    val q = Module(new Queue(new LS2WB, entries = 1, pipe = true))
    io.in.ready := q.io.enq.ready && ready_go
    q.io.enq.valid := io.in.valid && ready_go && io.out.ready
    q.io.enq.bits := to_enq
    io.out <> q.io.deq

    if (DebugConfig.enableTaps) {
        dontTouch(ready_go)
        dontTouch(byte_off)
        dontTouch(baseStrobe)
        dontTouch(wdata)

        DebugSignals.tap(this, ready_go.asUInt, "lsu.ready_go")
        DebugSignals.tap(this, byte_off, "lsu.byte_off")
        DebugSignals.tap(this, baseStrobe, "lsu.baseStrobe")
        DebugSignals.tap(this, wdata, "lsu.wdata")
        DebugSignals.tap(this, io.in.valid.asUInt, "lsu.io.in.valid")
        DebugSignals.tap(this, io.in.ready.asUInt, "lsu.io.in.ready")
        DebugSignals.tap(this, io.in.bits.asUInt, "lsu.io.in.bits")
        DebugSignals.tap(this, io.out.valid.asUInt, "lsu.io.out.valid")
        DebugSignals.tap(this, io.out.ready.asUInt, "lsu.io.out.ready")
        DebugSignals.tap(this, io.out.bits.asUInt, "lsu.io.out.bits")
        DebugSignals.tap(this, io.memReq.valid.asUInt, "lsu.io.memReq.valid")
        DebugSignals.tap(this, io.memReq.ready.asUInt, "lsu.io.memReq.ready")
        DebugSignals.tap(this, io.memReq.bits.asUInt, "lsu.io.memReq.bits")
        DebugSignals.tap(this, io.memReq.bits.addr, "lsu.addr")
    }
}
