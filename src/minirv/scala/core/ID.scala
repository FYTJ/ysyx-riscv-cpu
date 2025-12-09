package minirv.scala.core
import chisel3._
import chisel3.util.{Cat, BitPat, ListLookup, MuxLookup, Decoupled, Queue}
import minirv.scala.core.ctrl._
import minirv.scala.common._

class IDU extends Module {
    val io = IO(new Bundle {
        val in = Flipped(Decoupled(new IF2ID))
        val memResp = Flipped(Decoupled(new MemResp))
        val rfRaddr1 = Output(UInt(5.W))
        val rfRaddr2 = Output(UInt(5.W))
        val rfRdata1 = Input(UInt(32.W))
        val rfRdata2 = Input(UInt(32.W))

        val br_taken = Output(Bool())
        val br_target = Output(UInt(32.W))

        val alu_result = Input(UInt(32.W))
        val LS_valid = Input(Bool())
        val LS_rfWen = Input(Bool())
        val LS_rfWaddr = Input(UInt(5.W))
        val LS_res_from_mem = Input(Bool())
        val LS_alu_result = Input(UInt(32.W))
        val WB_valid = Input(Bool())
        val WB_rfWen = Input(Bool())
        val WB_rfWaddr = Input(UInt(5.W))
        val WB_rfWdata = Input(UInt(32.W))
        val out = Decoupled(new ID2EX)
    })
    val load_use_sign = Wire(Bool())
    val ready_go = !io.in.valid || !load_use_sign && io.memResp.fire

    val inst = io.memResp.bits.rdata
    io.memResp.ready := 1.B

    val opcode = inst(6, 0)
    val rd     = inst(11, 7)
    val funct3 = inst(14, 12)
    val funct7 = inst(31, 25)
    val rs1    = inst(19, 15)
    val rs2    = inst(24, 20)

    val inst_table = Array(
        BitPat("b?????????????????000?????0010011") -> // addi
            List(InstType.I.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b?????????????????010?????0010011") -> // slti
            List(InstType.I.asUInt, AluOp.SLT.asUInt, 0.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b?????????????????011?????0010011") -> // sltiu
            List(InstType.I.asUInt, AluOp.SLTU.asUInt, 0.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b?????????????????100?????0010011") -> // xori
            List(InstType.I.asUInt, AluOp.XOR.asUInt, 0.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b?????????????????110?????0010011") -> // ori 
            List(InstType.I.asUInt, AluOp.OR.asUInt, 0.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b?????????????????111?????0010011") -> // andi
            List(InstType.I.asUInt, AluOp.AND.asUInt, 0.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b0000000??????????001?????0010011") -> // slli
            List(InstType.I.asUInt, AluOp.SLL.asUInt, 0.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b0000000??????????101?????0010011") -> // srli
            List(InstType.I.asUInt, AluOp.SRL.asUInt, 0.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b0100000??????????101?????0010011") -> // srai
            List(InstType.I.asUInt, AluOp.SRA.asUInt, 0.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b?????????????????000?????0000011") -> // lb  
            List(InstType.I.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 1.U, 1.U, 0.U),
        BitPat("b?????????????????001?????0000011") -> // lh  
            List(InstType.I.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 1.U, 1.U, 0.U),
        BitPat("b?????????????????010?????0000011") -> // lw  
            List(InstType.I.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 1.U, 1.U, 0.U),
        BitPat("b?????????????????100?????0000011") -> // lbu 
            List(InstType.I.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 1.U, 1.U, 0.U),
        BitPat("b?????????????????101?????0000011") -> // lhu 
            List(InstType.I.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 1.U, 1.U, 0.U),
        BitPat("b?????????????????000?????1100111") -> // jalr
            List(InstType.I.asUInt, AluOp.JALR.asUInt, 1.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b?????????????????????????0110111") -> // lui 
            List(InstType.U.asUInt, AluOp.LUI.asUInt, 0.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b?????????????????????????0010111") -> // auipc
            List(InstType.U.asUInt, AluOp.ADD.asUInt, 1.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b0000000??????????000?????0110011") -> // add 
            List(InstType.R.asUInt, AluOp.ADD.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0100000??????????000?????0110011") -> // sub 
            List(InstType.R.asUInt, AluOp.SUB.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000000??????????001?????0110011") -> // sll 
            List(InstType.R.asUInt, AluOp.SLL.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000000??????????010?????0110011") -> // slt 
            List(InstType.R.asUInt, AluOp.SLT.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000000??????????011?????0110011") -> // sltu
            List(InstType.R.asUInt, AluOp.SLTU.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000000??????????100?????0110011") -> // xor 
            List(InstType.R.asUInt, AluOp.XOR.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000000??????????101?????0110011") -> // srl 
            List(InstType.R.asUInt, AluOp.SRL.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0100000??????????101?????0110011") -> // sra 
            List(InstType.R.asUInt, AluOp.SRA.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000000??????????110?????0110011") -> // or  
            List(InstType.R.asUInt, AluOp.OR.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000000??????????111?????0110011") -> // and 
            List(InstType.R.asUInt, AluOp.AND.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000001??????????000?????0110011") -> // mul 
            List(InstType.R.asUInt, AluOp.MUL.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000001??????????100?????0110011") -> // div 
            List(InstType.R.asUInt, AluOp.DIV.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000001??????????101?????0110011") -> // divu
            List(InstType.R.asUInt, AluOp.DIVU.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000001??????????001?????0110011") -> // mulh
            List(InstType.R.asUInt, AluOp.MULH.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000001??????????011?????0110011") -> // mulhu
            List(InstType.R.asUInt, AluOp.MULHU.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000001??????????110?????0110011") -> // rem 
            List(InstType.R.asUInt, AluOp.REM.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b0000001??????????111?????0110011") -> // remu
            List(InstType.R.asUInt, AluOp.REMU.asUInt, 0.U, 0.U, 1.U, 0.U, 0.U),
        BitPat("b?????????????????000?????0100011") -> // sb  
            List(InstType.S.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 0.U, 0.U, 1.U),
        BitPat("b?????????????????001?????0100011") -> // sh  
            List(InstType.S.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 0.U, 0.U, 1.U),
        BitPat("b?????????????????010?????0100011") -> // sw  
            List(InstType.S.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 0.U, 0.U, 1.U),
        BitPat("b?????????????????????????1101111") -> // jal 
            List(InstType.J.asUInt, AluOp.JAL.asUInt, 1.U, 1.U, 1.U, 0.U, 0.U),
        BitPat("b?????????????????000?????1100011") -> // beq 
            List(InstType.B.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 0.U, 0.U, 0.U),
        BitPat("b?????????????????001?????1100011") -> // bne 
            List(InstType.B.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 0.U, 0.U, 0.U),
        BitPat("b?????????????????100?????1100011") -> // blt 
            List(InstType.B.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 0.U, 0.U, 0.U),
        BitPat("b?????????????????101?????1100011") -> // bge 
            List(InstType.B.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 0.U, 0.U, 0.U),
        BitPat("b?????????????????110?????1100011") -> // bltu
            List(InstType.B.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 0.U, 0.U, 0.U),
        BitPat("b?????????????????111?????1100011") -> // bgeu
            List(InstType.B.asUInt, AluOp.ADD.asUInt, 0.U, 1.U, 0.U, 0.U, 0.U),
        BitPat("b00000000000100000000000001110011") -> // ebreak
            List(InstType.N.asUInt, AluOp.PASS.asUInt, 0.U, 0.U, 0.U, 0.U, 0.U)
    )

    val dflt: List[UInt] = List(
        InstType.N.asUInt,  // type
        AluOp.PASS.asUInt,  // aluOp
        0.U,                // src1_is_pc
        0.U,                // src2_is_imm
        0.U,                // rfWen
        0.U,                // res_from_mem
        0.U                 // memWen
    )

    val imm_table: Seq[(UInt, UInt)] = Seq(
        InstType.R.asUInt -> 0.U(32.W),
        InstType.I.asUInt -> BitUtils.sext(inst(31, 20), 12),
        InstType.S.asUInt -> BitUtils.sext(Cat(inst(31, 25), inst(11, 7)), 12),
        InstType.B.asUInt -> BitUtils.sext(Cat(inst(31), inst(7), inst(30, 25), inst(11, 8), 0.U(1.W)), 13),
        InstType.U.asUInt -> Cat(inst(31, 12), 0.U(12.W)),
        InstType.J.asUInt -> BitUtils.sext(Cat(inst(31), inst(19, 12), inst(20), inst(30, 21), 0.U(1.W)), 21)
    )

    val decoded = ListLookup(inst, dflt, inst_table)
    val inst_jal = inst === BitPat("b?????????????????????????1101111")
    val inst_jalr = inst === BitPat("b?????????????????000?????1100111")
    val inst_beq = inst === BitPat("b?????????????????000?????1100011")
    val inst_bne = inst === BitPat("b?????????????????001?????1100011")
    val inst_blt = inst === BitPat("b?????????????????100?????1100011")
    val inst_bge = inst === BitPat("b?????????????????101?????1100011")
    val inst_bltu = inst === BitPat("b?????????????????110?????1100011")
    val inst_bgeu = inst === BitPat("b?????????????????111?????1100011")
    val ebreak = inst === BitPat("b00000000000100000000000001110011")

    val to_enq = Wire(new ID2EX)
    to_enq.pc := io.in.bits.pc
    
    val instType = InstType.safe(decoded(0))._1
    to_enq.aluOp := AluOp.safe(decoded(1))._1
    to_enq.src1_is_pc := decoded(2).asTypeOf(Bool())
    to_enq.src2_is_imm := decoded(3).asTypeOf(Bool())
    to_enq.rfWen := decoded(4).asTypeOf(Bool())
    to_enq.res_from_mem := decoded(5).asTypeOf(Bool())
    to_enq.memWen := decoded(6).asTypeOf(Bool())

    to_enq.imm := MuxLookup(instType.asUInt, 0.U(32.W))(imm_table)

    to_enq.rfWaddr := rd
    io.rfRaddr1 := rs1
    io.rfRaddr2 := rs2
    to_enq.mem_ctrl := funct3
    when (io.out.valid && io.out.bits.rfWen && !io.out.bits.res_from_mem && (rs1 === io.out.bits.rfWaddr) && (rs1 =/= 0.U)) {
        to_enq.value1 := io.alu_result
    } .elsewhen (io.LS_valid && io.LS_rfWen && !io.LS_res_from_mem && (rs1 === io.LS_rfWaddr) && (rs1 =/= 0.U)) {
        to_enq.value1 := io.LS_alu_result
    } .elsewhen (io.WB_valid && io.WB_rfWen && (rs1 === io.WB_rfWaddr) && (rs1 =/= 0.U)) {
        to_enq.value1 := io.WB_rfWdata
    } .otherwise {
        to_enq.value1 := io.rfRdata1
    }

    when (io.out.valid && io.out.bits.rfWen && !io.out.bits.res_from_mem && (rs2 === io.out.bits.rfWaddr) && (rs2 =/= 0.U)) {
        to_enq.value2 := io.alu_result
    } .elsewhen (io.LS_valid && io.LS_rfWen && !io.LS_res_from_mem && (rs2 === io.LS_rfWaddr) && (rs2 =/= 0.U)) {
        to_enq.value2 := io.LS_alu_result
    } .elsewhen (io.WB_valid && io.WB_rfWen && (rs2 === io.WB_rfWaddr) && (rs2 =/= 0.U)) {
        to_enq.value2 := io.WB_rfWdata
    } .otherwise {
        to_enq.value2 := io.rfRdata2
    }
    to_enq.ebreak := ebreak

    load_use_sign := io.in.valid && (
        (io.rfRaddr1 === io.out.bits.rfWaddr) && io.out.bits.rfWen && io.out.bits.res_from_mem && io.out.valid ||
        (io.rfRaddr2 === io.out.bits.rfWaddr) && io.out.bits.rfWen && io.out.bits.res_from_mem && io.out.valid ||
        (io.rfRaddr1 === io.LS_rfWaddr) && io.LS_rfWen && io.LS_res_from_mem && io.LS_valid ||
        (io.rfRaddr2 === io.LS_rfWaddr) && io.LS_rfWen && io.LS_res_from_mem && io.LS_valid
    )

    io.br_taken := inst_jal || inst_jalr ||
        (inst_beq && (to_enq.value1 === to_enq.value2)) ||
        (inst_bne && (to_enq.value1 =/= to_enq.value2)) ||
        (inst_blt && (to_enq.value1.asSInt < to_enq.value2.asSInt)) ||
        (inst_bge && (to_enq.value1.asSInt >= to_enq.value2.asSInt)) ||
        (inst_bltu && (to_enq.value1.asUInt < to_enq.value2.asUInt)) ||
        (inst_bgeu && (to_enq.value1.asUInt >= to_enq.value2.asUInt))
    io.br_target := Mux(inst_jalr, (to_enq.imm.asSInt + to_enq.value1.asSInt), to_enq.imm.asSInt + to_enq.pc.asSInt).asUInt

    val q = Module(new Queue(new ID2EX, entries = 1, pipe = true))
    io.in.ready := q.io.enq.ready && ready_go
    q.io.enq.valid := io.in.valid && ready_go && io.out.ready
    q.io.enq.bits := to_enq
    io.out <> q.io.deq

    dontTouch(ready_go)
    dontTouch(inst_beq)
    dontTouch(inst_bne)
    dontTouch(inst_blt)
    dontTouch(inst_bge)
    dontTouch(inst_bltu)
    dontTouch(inst_bgeu)
    dontTouch(inst_jal)
    dontTouch(inst_jalr)
    dontTouch(rs1)
    dontTouch(rs2)
    dontTouch(rd)
    dontTouch(funct3)

    DebugSignals.tap(this, inst, "idu.inst")
    DebugSignals.tap(this, io.br_taken.asUInt, "idu.br_taken")
    DebugSignals.tap(this, io.br_target, "idu.br_target")
    DebugSignals.tap(this, io.rfRaddr1, "idu.rs1")
    DebugSignals.tap(this, io.rfRaddr2, "idu.rs2")
    DebugSignals.tap(this, ready_go.asUInt, "idu.ready_go")
    DebugSignals.tap(this, inst_beq.asUInt, "idu.inst_beq")
    DebugSignals.tap(this, inst_bne.asUInt, "idu.inst_bne")
    DebugSignals.tap(this, inst_blt.asUInt, "idu.inst_blt")
    DebugSignals.tap(this, inst_bge.asUInt, "idu.inst_bge")
    DebugSignals.tap(this, inst_bltu.asUInt, "idu.inst_bltu")
    DebugSignals.tap(this, inst_bgeu.asUInt, "idu.inst_bgeu")
    DebugSignals.tap(this, inst_jal.asUInt, "idu.inst_jal")
    DebugSignals.tap(this, inst_jalr.asUInt, "idu.inst_jalr")
    DebugSignals.tap(this, rd, "idu.rd")
    DebugSignals.tap(this, funct3, "idu.funct3")
    DebugSignals.tap(this, io.in.valid.asUInt, "idu.io.in.valid")
    DebugSignals.tap(this, io.in.ready.asUInt, "idu.io.in.ready")
    DebugSignals.tap(this, io.in.bits.asUInt, "idu.io.in.bits")
    DebugSignals.tap(this, io.memResp.valid.asUInt, "idu.io.memResp.valid")
    DebugSignals.tap(this, io.memResp.ready.asUInt, "idu.io.memResp.ready")
    DebugSignals.tap(this, io.memResp.bits.asUInt, "idu.io.memResp.bits")
    DebugSignals.tap(this, io.out.valid.asUInt, "idu.io.out.valid")
    DebugSignals.tap(this, io.out.ready.asUInt, "idu.io.out.ready")
    DebugSignals.tap(this, io.out.bits.asUInt, "idu.io.out.bits")
    DebugSignals.tap(this, io.alu_result, "idu.io.alu_result")
    DebugSignals.tap(this, io.LS_valid.asUInt, "idu.io.LS_valid")
    DebugSignals.tap(this, io.LS_rfWen.asUInt, "idu.io.LS_rfWen")
    DebugSignals.tap(this, io.LS_rfWaddr, "idu.io.LS_rfWaddr")
    DebugSignals.tap(this, io.LS_res_from_mem.asUInt, "idu.io.LS_res_from_mem")
    DebugSignals.tap(this, io.LS_alu_result, "idu.io.LS_alu_result")
    DebugSignals.tap(this, io.WB_valid.asUInt, "idu.io.WB_valid")
    DebugSignals.tap(this, io.WB_rfWen.asUInt, "idu.io.WB_rfWen")
    DebugSignals.tap(this, io.WB_rfWaddr, "idu.io.WB_rfWaddr")
    DebugSignals.tap(this, io.WB_rfWdata, "idu.io.WB_rfWdata")
}
