import chisel3._
import circt.stage.ChiselStage
import minirv.scala.CpuTop

object Elaborate extends App {
    val targetDir = "build"
    
    println(s"Generating Verilog for CpuTop to directory: $targetDir")
    
    ChiselStage.emitSystemVerilogFile(
        new CpuTop,
        Array("--target-dir", targetDir)
    )
    
    println("Verilog generation completed!")
    println(s"Generated files:")
    println(s"  - $targetDir/CpuTop.v")
    println(s"  - $targetDir/CpuTop.fir")
}