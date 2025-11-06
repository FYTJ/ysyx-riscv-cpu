package minirv.scala.core
import chisel3._
import chisel3.util.{Decoupled, Queue}
import minirv.scala.core.ctrl._

class EXU extends Module {
    val io = IO(new Bundle {
        val in = Flipped(Decoupled(new ID2EX))
        val out = Decoupled(new EX2LS)
        val alu_result = Output(UInt(32.W))
    })

    val ready_go = Wire(Bool())
    ready_go := 1.B

    val alu_A = Mux(io.in.bits.src1_is_pc, io.in.bits.pc, io.in.bits.value1)
    val alu_B = Mux(io.in.bits.src2_is_imm, io.in.bits.imm, io.in.bits.value2)

    val alu = Module(new Alu())
    alu.io.aluOp := io.in.bits.aluOp
    alu.io.a := alu_A.asSInt
    alu.io.b := alu_B.asSInt
    io.alu_result := alu.io.res.asUInt

    val to_enq = Wire(new EX2LS)
    to_enq.pc := io.in.bits.pc
    to_enq.alu_result := alu.io.res.asUInt
    to_enq.res_from_mem := io.in.bits.res_from_mem
    to_enq.rfWen := io.in.bits.rfWen
    to_enq.rfWaddr := io.in.bits.rfWaddr
    to_enq.memWen := io.in.bits.memWen
    to_enq.mem_ctrl := io.in.bits.mem_ctrl
    to_enq.value2 := io.in.bits.value2
    to_enq.ebreak := io.in.bits.ebreak

    val q = Module(new Queue(new EX2LS, entries = 1, pipe = true))
    io.in.ready := q.io.enq.ready && ready_go
    q.io.enq.valid := io.in.valid && ready_go && io.out.ready
    q.io.enq.bits := to_enq
    io.out <> q.io.deq

    dontTouch(ready_go)
    dontTouch(alu_A)
    dontTouch(alu_B)
}