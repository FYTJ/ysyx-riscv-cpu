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
}