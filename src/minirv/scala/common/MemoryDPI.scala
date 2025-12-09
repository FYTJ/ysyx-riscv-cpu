package minirv.scala.common
import chisel3._
import chisel3.util.{HasBlackBoxResource, Decoupled}

class MemoryDPI extends BlackBox with HasBlackBoxResource {
    val io = IO(new Bundle {
        val clock = Input(Clock())
        val reset = Input(Reset())

        val ren = Input(Bool())
        val addr = Input(UInt(32.W))
        val wen = Input(Bool())
        val wstrb = Input(UInt(4.W))
        val wdata = Input(UInt(32.W))

        val rdata = Output(UInt(32.W))
        val wfinish = Output(Bool())
    })
    addResource("/minirv/MemoryDPI.v")
}

class MemReq extends Bundle {
    val ren = Bool()
    val wen = Bool()
    val addr = UInt(32.W)
    val wstrb = UInt(4.W)
    val wdata = UInt(32.W)
}

class MemResp extends Bundle {
    val rdata = UInt(32.W)
    val wfinish = Bool()
}

class MemoryInterface extends Module {
    val io = IO(new Bundle {
        val req = Flipped(Decoupled(new MemReq))
        val resp = Decoupled(new MemResp)
    })

    val memory_dpi = Module(new MemoryDPI)
    
    memory_dpi.io.clock := clock
    memory_dpi.io.reset := reset
    io.req.ready := 1.B
    
    memory_dpi.io.ren := io.req.bits.ren && io.req.valid
    memory_dpi.io.wen := io.req.bits.wen && io.req.valid
    memory_dpi.io.addr := io.req.bits.addr
    memory_dpi.io.wstrb := io.req.bits.wstrb
    memory_dpi.io.wdata := io.req.bits.wdata

    io.resp.valid := 1.B
    io.resp.bits.rdata := memory_dpi.io.rdata
    io.resp.bits.wfinish := memory_dpi.io.wfinish
}

object Memory_DPI {
    def createMemoryInterface(): MemoryInterface = {
        Module(new MemoryInterface)
    }
}
