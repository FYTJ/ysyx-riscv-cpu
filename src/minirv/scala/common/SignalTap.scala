package minirv.scala.common
import chisel3._
import chisel3.util.HasBlackBoxResource
import chisel3.experimental.{IntParam, StringParam}

class SignalTap(name: String, width: Int, id: Int) extends BlackBox(Map(
  "NAME" -> StringParam(name),
  "WIDTH" -> IntParam(width),
  "ID" -> IntParam(id)
)) with HasBlackBoxResource {
  override def desiredName: String = "SignalDPI"
  val io = IO(new Bundle {
    val clock = Input(Clock())
    val reset = Input(Reset())
    val in = Input(UInt(width.W))
  })
  addResource("/minirv/SignalDPI.v")
}

object DebugSignals {
  private var nextId: Int = 0
  def tap(m: Module, sig: UInt, path: String): Unit = {
    if (DebugConfig.enableTaps) {
      val t = Module(new SignalTap(path, sig.getWidth, nextId))
      nextId = nextId + 1
      t.io.clock := m.clock
      t.io.reset := m.reset
      t.io.in := sig
    }
  }
}
