package minirv.scala.core

import chisel3._
import chisel3.util.{Decoupled, Cat, Queue}
import minirv.scala.core.ctrl._
import minirv.scala.common._

object Status extends ChiselEnum {
    val IDLE, BUSY = Value
}

class DIV extends Module {
    val io = IO(new Bundle {
        val in = Flipped(Decoupled(new DivReq()))
        val out = Decoupled(new DivResp())
    })

    val dividend = RegInit(0.U(32.W))
    val divisor = RegInit(0.U(32.W))
    val divOp = RegInit(DivOp.DIV)

    val clk_counter = RegInit(0.U(6.W))
    val status = RegInit(Status.IDLE)

    val abs_dividend = Wire(UInt(32.W))
    abs_dividend := Mux(divOp === DivOp.DIV || divOp === DivOp.REM, BitUtils.abs(dividend), dividend)
    val abs_divisor = Wire(UInt(32.W))
    abs_divisor := Mux(divOp === DivOp.DIV || divOp === DivOp.REM, BitUtils.abs(divisor), divisor)
    val sign_dividend = Wire(Bool())
    sign_dividend := Mux(divOp === DivOp.DIV || divOp === DivOp.REM, dividend(31), 0.B)
    val sign_divisor = Wire(Bool())
    sign_divisor := Mux(divOp === DivOp.DIV || divOp === DivOp.REM, divisor(31), 0.B)
    val sign_quotient = Wire(Bool())
    sign_quotient := sign_dividend ^ sign_divisor
    val sign_remainder = Wire(Bool())
    sign_remainder := sign_dividend
    val num_quotient = RegInit(0.U(32.W))
    val num_remainder = RegInit(0.U(32.W))
    val zext_dividend = Wire(UInt(64.W))
    zext_dividend := BitUtils.zext(abs_dividend, 32, 64)
    val zext_divisor = Wire(UInt(33.W))
    zext_divisor := BitUtils.zext(abs_divisor, 32, 33)

    def div_iter(dividend: UInt, divisor: UInt): (Bool, UInt) = {
        val sub = Wire(UInt(33.W))
        sub := dividend - divisor
        val quotient_bit = (sub(32) === 0.B)
        val remainder = Wire(UInt(32.W))
        remainder := Mux(quotient_bit, sub(31, 0), dividend(31, 0))
        (quotient_bit, remainder)
    }

    val new_dividend = RegInit(0.U(33.W))

    when (io.in.fire) {
        status := Status.BUSY
        dividend := io.in.bits.dividend
        divisor := io.in.bits.divisor
        divOp := io.in.bits.divOp
        clk_counter := 0.U
        val zext_in_dividend = Wire(UInt(64.W))
        zext_in_dividend := BitUtils.zext(Mux(io.in.bits.divOp === DivOp.DIV || io.in.bits.divOp === DivOp.REM, BitUtils.abs(io.in.bits.dividend), io.in.bits.dividend), 32, 64)
        new_dividend := zext_in_dividend(63, 31)
    }.elsewhen (clk_counter < 32.U && status === Status.BUSY) {
        when (divisor === 0.U) {
            clk_counter := 32.U
            num_quotient := Mux(sign_quotient, (1.U(32.W)), "hFFFFFFFF".U)
            num_remainder := Mux((divOp === DivOp.DIV || divOp === DivOp.REM) && sign_remainder, (~dividend + 1.U), dividend)
        }.otherwise {
            val (quotient_bit, remainder) = div_iter(new_dividend, zext_divisor)
            num_quotient := num_quotient | quotient_bit << (31.U - clk_counter)
            num_remainder := remainder
            new_dividend := Mux(clk_counter === 31.U, 0.U, Cat(remainder(31, 0), zext_dividend(30.U - clk_counter)))
            clk_counter := clk_counter + 1.U
        }
    }.elsewhen (clk_counter === 32.U && io.out.fire) {
        status := Status.IDLE
        clk_counter := 0.U
        num_quotient := 0.U
    }

    io.in.ready := status === Status.IDLE
    io.out.valid := (clk_counter === 32.U) && (status === Status.BUSY) 
    io.out.bits.quotient := Mux((divOp === DivOp.DIV || divOp === DivOp.REM) && sign_quotient, (~num_quotient + 1.U), num_quotient)
    io.out.bits.remainder := Mux((divOp === DivOp.DIV || divOp === DivOp.REM) && sign_remainder, (~num_remainder + 1.U), num_remainder)
}