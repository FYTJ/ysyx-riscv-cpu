package minirv.scala.common

object DebugConfig {
  private def truthy(v: String): Boolean = {
    val s = v.toLowerCase
    s == "1" || s == "true" || s == "on" || s == "yes"
  }
  val enableTaps: Boolean = sys.env.get("NPC_DEBUG_MODE").exists(truthy)
}

