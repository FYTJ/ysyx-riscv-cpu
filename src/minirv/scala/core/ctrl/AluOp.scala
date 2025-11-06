package minirv.scala.core.ctrl
import chisel3._

object AluOp extends ChiselEnum {
    val ADD, SUB, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU, MUL, MULH, MULHU, DIV, DIVU, REM, REMU, LUI, AUIPC, JAL, JALR, PASS = Value
}

object DivOp extends ChiselEnum {
    val DIV, DIVU, REM, REMU = Value
}