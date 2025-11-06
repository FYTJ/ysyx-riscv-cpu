package minirv.scala.core
import chisel3._
import chisel3.util.{switch, is}
import minirv.scala.core.ctrl._
import upickle.default

class Alu extends Module {
    val io = IO(new Bundle{
        val aluOp = Input(AluOp())
        val a = Input(SInt(32.W))
        val b = Input(SInt(32.W))
        val res = Output(SInt(32.W))
    })
    
    io.res := 0.S
    switch(io.aluOp){
        is(AluOp.ADD, AluOp.SUB){
            io.res := io.a + Mux(io.aluOp === AluOp.SUB, (~io.b.asSInt + 1.S), io.b.asSInt)
        }
        is(AluOp.AND){
            io.res := io.a & io.b
        }
        is(AluOp.OR){
            io.res := io.a | io.b
        }
        is(AluOp.XOR){
            io.res := io.a ^ io.b
        }
        is(AluOp.SLL){
            io.res := (io.a.asUInt << io.b.asUInt(4, 0)).asSInt
        }
        is(AluOp.SRL){
            io.res := (io.a.asUInt >> io.b.asUInt(4, 0)).asSInt
        }
        is(AluOp.SRA){
            io.res := (io.a >> io.b.asUInt(4, 0)).asSInt
        }
        is(AluOp.SLT){
            io.res := (io.a < io.b).asSInt
        }
        is(AluOp.SLTU){
            io.res := (io.a.asUInt < io.b.asUInt).asSInt
        }
        is(AluOp.MUL, AluOp.MULH){
            val prod = io.a * io.b
            io.res := Mux(io.aluOp === AluOp.MUL, prod(31, 0).asSInt, prod(63, 32).asSInt)
        }
        is(AluOp.MULHU){
            io.res := (io.a.asUInt * io.b.asUInt)(63, 32).asSInt
        }
        is(AluOp.DIV){
            io.res := io.a / io.b
        }
        is(AluOp.DIVU){
            io.res := (io.a.asUInt / io.b.asUInt).asSInt
        }
        is(AluOp.REM){
            io.res := io.a % io.b
        }
        is(AluOp.REMU){
            io.res := (io.a.asUInt % io.b.asUInt).asSInt
        }
        is(AluOp.LUI){
            io.res := io.b
        }
        is(AluOp.JAL, AluOp.JALR){
            io.res := io.a + 4.S
        }
        is(AluOp.PASS){
            io.res := io.a
        }
    }
}