import chisel3._
import circt.stage.ChiselStage
import minirv.scala.CpuTop

class SimTop extends Module {
    val io = IO(new Bundle {
        val exit = Output(Bool())
        val exit_code = Output(UInt(32.W))
        val debug_r10 = Output(UInt(32.W))
        val debug_pc = Output(UInt(32.W))
        val debug_rf_wen = Output(Bool())
        val debug_rf_waddr = Output(UInt(5.W))
        val debug_rf_wdata = Output(UInt(32.W))
    })
    
    val cpu = Module(new CpuTop)
    
    val cycle_count = RegInit(0.U(32.W))
    cycle_count := cycle_count + 1.U
        
    val cpu_exit = cpu.io.ebreak
    
    io.exit := cpu_exit
    io.exit_code := Mux(cpu_exit, 0.U, 1.U)
    io.debug_r10 := cpu.io.debug_r10
    io.debug_pc := cpu.io.debug_pc
    io.debug_rf_wen := cpu.io.debug_rf_wen
    io.debug_rf_waddr := cpu.io.debug_rf_waddr
    io.debug_rf_wdata := cpu.io.debug_rf_wdata
}

object SimTop extends App {
    val targetDirArgIdx = args.indexOf("--target-dir")
    val targetDir =
      if (targetDirArgIdx >= 0 && targetDirArgIdx + 1 < args.length) args(targetDirArgIdx + 1)
      else "build"

    println(s"[SimTop] emit to target-dir: ${targetDir}")

    ChiselStage.emitSystemVerilogFile(
        new SimTop,
        Array("--target-dir", targetDir)
    )
}