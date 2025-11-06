package minirv.scala.common
import chisel3._
import chisel3.util.{Cat, Fill}

object BitUtils{
    def sext(input: UInt, fromWidth: Int, toWidth: Int = 32): UInt = {
        require(fromWidth <= input.getWidth, s"fromWidth=$fromWidth > input.width=${input.getWidth}")
        require(toWidth >= fromWidth, s"toWidth=$toWidth < fromWidth=$fromWidth")
        Cat(Fill(toWidth - fromWidth, input(fromWidth - 1)), input)
    }

    def zext(input: UInt, fromWidth: Int, toWidth: Int = 32): UInt = {
        require(fromWidth <= input.getWidth, s"fromWidth=$fromWidth > input.width=${input.getWidth}")
        require(toWidth >= fromWidth, s"toWidth=$toWidth < fromWidth=$fromWidth")
        Cat(Fill(toWidth - fromWidth, 0.U), input)
    }

    def abs(input: UInt, width: Int = 32): UInt = {
        val sign = input(width - 1)
        Mux(sign, -input, input)
    }
}
