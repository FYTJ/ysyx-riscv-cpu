package minirv.scala.core

import chisel3._
import minirv.scala.core.ctrl._

class IF2ID extends Bundle {
    val pc = UInt(32.W)
}

class ID2EX extends Bundle {
    val pc = UInt(32.W)
    val aluOp = AluOp()
    val imm = UInt(32.W)
    val rfWen = Bool()
    val rfWaddr = UInt(5.W)
    val src1_is_pc = Bool()
    val src2_is_imm = Bool()
    val res_from_mem = Bool()
    val memWen = Bool()
    val mem_ctrl = UInt(3.W)
    val value1 = UInt(32.W)
    val value2 = UInt(32.W)
    val ebreak = Bool()
}

class EX2LS extends Bundle {
    val pc = UInt(32.W)
    val alu_result = UInt(32.W)
    val res_from_mem = Bool()
    val rfWen = Bool()
    val rfWaddr = UInt(5.W)
    val memWen = Bool()
    val mem_ctrl = UInt(3.W)
    val value2 = UInt(32.W)
    val ebreak = Bool()
}

class LS2WB extends Bundle {
    val pc = UInt(32.W)
    val mem_ctrl = UInt(3.W)
    val alu_result = UInt(32.W)
    val res_from_mem = Bool()
    val rfWen = Bool()
    val rfWaddr = UInt(5.W)
    val ebreak = Bool()
}

class DivReq extends Bundle {
    val divOp = DivOp()
    val dividend = UInt(32.W)
    val divisor = UInt(32.W)
}

class DivResp extends Bundle {
    val quotient = UInt(32.W)
    val remainder = UInt(32.W)
}