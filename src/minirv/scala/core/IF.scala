package minirv.scala.core
import chisel3._
import chisel3.util.{Decoupled, Queue}
import minirv.scala.common._

class IFU extends Module {
    val io = IO(new Bundle {
        val memReq = Decoupled(new MemReq)
        val br_taken = Input(Bool())
        val br_target = Input(UInt(32.W))
        val ebreak = Input(Bool())
        val out = Decoupled(new IF2ID)
    })
    val ready_go = !io.ebreak && io.memReq.fire

    val pc = RegInit((0x80000000 - 4).S(32.W).asUInt)
    val seq_pc = Wire(UInt(32.W))
    val next_pc = Wire(UInt(32.W))
    
    when (ready_go && io.out.ready) {
        seq_pc := pc + 4.U
        next_pc := Mux(io.br_taken, io.br_target, seq_pc)
        pc := next_pc 
    }.otherwise {
        seq_pc := pc
        next_pc := pc
    }
    
    io.memReq.valid := !reset.asBool
    io.memReq.bits.ren := 1.B
    io.memReq.bits.wen := 0.B
    io.memReq.bits.addr := next_pc
    io.memReq.bits.wstrb := 0.U
    io.memReq.bits.wdata := 0.U
    
    val to_enq = Wire(new IF2ID)
    to_enq.pc := next_pc

    val q = Module(new Queue(new IF2ID, entries = 1, pipe = true))
    q.io.enq.valid := ready_go && io.out.ready
    q.io.enq.bits := to_enq
    io.out <> q.io.deq

    dontTouch(ready_go)
    dontTouch(seq_pc)
    dontTouch(next_pc)

    DebugSignals.tap(this, pc, "ifu.pc")
    DebugSignals.tap(this, seq_pc, "ifu.seq_pc")
    DebugSignals.tap(this, next_pc, "ifu.next_pc")
    DebugSignals.tap(this, ready_go.asUInt, "ifu.ready_go")
    DebugSignals.tap(this, io.br_taken.asUInt, "ifu.io.br_taken")
    DebugSignals.tap(this, io.br_target, "ifu.io.br_target")
    DebugSignals.tap(this, io.ebreak.asUInt, "ifu.io.ebreak")
    DebugSignals.tap(this, io.memReq.valid.asUInt, "ifu.io.memReq.valid")
    DebugSignals.tap(this, io.memReq.ready.asUInt, "ifu.io.memReq.ready")
    DebugSignals.tap(this, io.memReq.bits.asUInt, "ifu.io.memReq.bits")
    DebugSignals.tap(this, io.out.valid.asUInt, "ifu.io.out.valid")
    DebugSignals.tap(this, io.out.ready.asUInt, "ifu.io.out.ready")
    DebugSignals.tap(this, io.out.bits.asUInt, "ifu.io.out.bits")
}
