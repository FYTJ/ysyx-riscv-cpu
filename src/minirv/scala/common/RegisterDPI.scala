package minirv.scala.common

import chisel3._
import chisel3.util.{HasBlackBoxResource}

class RegisterDPI extends BlackBox with HasBlackBoxResource {
    val io = IO(new Bundle {
        val clock = Input(Clock())
        val reset = Input(Reset())
        val RF_value = Input(Vec(32, UInt(32.W)))
    })
    addResource("/minirv/RegisterDPI.v")
}
