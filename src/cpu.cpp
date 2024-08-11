#include "cpu.h"
#include "allocator.h"
#include "utils.h"

#include "bus.h"

#include "opcodes.h"

#define F_Z 7
#define F_N 6
#define F_H 5
#define F_C 4

namespace {
    uint8_t interruptVectors[] = { 0x40, 0x48, 0x50, 0x58, 0x60 };
}

// Loggings
#ifdef LOGGING_ENABLED
const char* opcode_names[] = {
"NOP", "LD BC,nn", "LD (BC),A", "INC BC", "INC B", "DEC B", "LD B,n", "RLCA", "LD (nn),SP", "ADD HL,BC", "LD A,(BC)", "DEC BC", "INC C", "DEC C", "LD C,n", "RRCA",
"STOP", "LD DE,nn", "LD (DE),A", "INC DE", "INC D", "DEC D", "LD D,n", "RLA", "JR n", "ADD HL,DE", "LD A,(DE)", "DEC DE", "INC E", "DEC E", "LD E,n", "RRA",
"JR NZ,n", "LD HL,nn", "LD (HL+),A", "INC HL", "INC H", "DEC H", "LD H,n", "DAA", "JR Z,n", "ADD HL,HL", "LD A,(HLI)", "DEC HL", "INC L", "DEC L", "LD L,n", "CPL",
"JR NC,n", "LD SP,nn", "LD (HL-),A", "INC SP", "INC (HL)", "DEC (HL)", "LD (HL),n", "SCF", "JR C,n", "ADD HL,SP", "LD A,(HLD)", "DEC SP", "INC A", "DEC A", "LDA,n", "CCF",
"LD B,B", "LD B,C", "LD B,D", "LD B,E", "LD B,H", "LD B,L", "LD B,(HL)", "LD B,A", "LD C,B", "LD C,C", "LD C,D", "LD C,E", "LD C,H", "LD C,L", "LD C,(HL)", "LD C,A",
"LD D,B", "LD D,C", "LD D,D", "LD D,E", "LD D,H", "LD D,L", "LD D,(HL)", "LD D,A", "LD E,B", "LD E,C", "LD E,D", "LD E,E", "LD E,H", "LD E,L", "LD E,(HL)", "LD E,A",
"LD H,B", "LD H,C", "LD H,D", "LD H,E", "LD H,H", "LD H,L", "LD H,(HL)", "LD H,A", "LD L,B", "LD L,C", "LD L,D", "LD L,E", "LD L,H", "LD L,L", "LD L,(HL)", "LD L,A",
"LD (HL),B", "LD (HL),C", "LD (HL),D", "LD (HL),E", "LD (HL),H", "LD (HL),L", "HALT", "LD (HL),A", "LD A,B", "LD A,C", "LD A,D", "LD A,E", "LD A,H", "LD A,L", "LD A,(HL)", "LD A,A",
"ADD A,B", "ADD A,C", "ADD A,D", "ADD A,E", "ADD A,H", "ADD A,L", "ADD A,(HL)", "ADD A,A", "ADC A,B", "ADC A,C", "ADC A,D", "ADC A,E", "ADC A,H", "ADC A,L", "ADC A,(HL)", "ADC A,A",
"SUB B", "SUB C", "SUB D", "SUB E", "SUB H", "SUB L", "SUB (HL)", "SUB A", "SBC A,B", "SBC A,C", "SBC A,D", "SBC A,E", "SBC A,H", "SBC A,L", "SBC A,(HL)", "SBC A,A",
"AND B", "AND C", "AND D", "AND E", "AND H", "AND L", "AND (HL)", "AND A", "XOR B", "XOR C", "XOR D", "XOR E", "XOR H", "XOR L", "XOR (HL)", "XOR A",
"OR B", "OR C", "OR D", "OR E", "OR H", "OR L", "OR (HL)", "OR A", "CP B", "CP C", "CP D", "CP E", "CP H", "CP L", "CP (HL)", "CP A",
"RET NZ", "POP BC", "JP NZ,nn", "JP nn", "CALL NZ,nn", "PUSH BC", "ADD A,n", "RST ", "RET Z", "RET", "JP Z,nn", "cb opcode", "CALL Z,nn", "CALL nn", "ADC A,n", "RST 0x08",
"RET NC", "POP DE", "JP NC,nn", "unused opcode", "CALL NC,nn", "PUSH DE", "SUB n", "RST 0x10", "RET C", "RETI", "JP C,nn", "unused opcode", "CALL C,nn", "unused opcode", "SBC A,n", "RST 0x18",
"LD (0xFF00+n),A", "POP HL", "LD (0xFF00+C),A", "unused opcode", "unused opcode", "PUSH HL", "AND n", "RST 0x20", "ADD SP,n", "JP (HL)", "LD (nn),A", "unused opcode", "unused opcode", "unused opcode", "XOR n", "RST 0x28",
"LD A,(0xFF00+n)", "POP AF", "LD A,(0xFF00+C)", "DI", "unused opcode", "PUSH AF", "OR n", "RST 0x30", "LD HL,SP", "LD SP,HL", "LD A,(nn)", "EI", "unused opcode", "unused opcode", "CP n", "RST 0x38" };

const char* ex_opcode_names[] = {
"RLC B", "RLC C", "RLC D", "RLC E", "RLC H", "RLC L", "RLC (HL)", "RLC A", "RRC B", "RRC C", "RRC D", "RRC E", "RRC H", "RRC L", "RRC (HL)", "RRC A",
"RL B", "RL C", "RL D", "RL E", "RL H", "RL L ", "RL (HL)", "RL A", "RR B", "RR C", "RR D", "RR E", "RR H", "RR L", "RR (HL)", "RR A",
"SLA B", "SLA C", "SLA D", "SLA E", "SLA H", "SLA L", "SLA (HL)", "SLA A", "SRA B", "SRA C", "SRA D", "SRA E", "SRA H", "SRA L", "SRA (HL)", "SRA A",
"SWAP B", "SWAP C", "SWAP D", "SWAP E", "SWAP H", "SWAP L", "SWAP (HL)", "SWAP A", "SRL B", "SRL C", "SRL D", "SRL E", "SRL H", "SRL L", "SRL (HL)", "SRL A",
"BIT 0 B", "BIT 0 C", "BIT 0 D", "BIT 0 E", "BIT 0 H", "BIT 0 L", "BIT 0 (HL)", "BIT 0 A", "BIT 1 B", "BIT 1 C", "BIT 1 D", "BIT 1 E", "BIT 1 H", "BIT 1 L", "BIT 1 (HL)", "BIT 1 A",
"BIT 2 B", "BIT 2 C", "BIT 2 D", "BIT 2 E", "BIT 2 H", "BIT 2 L", "BIT 2 (HL)", "BIT 2 A", "BIT 3 B", "BIT 3 C", "BIT 3 D", "BIT 3 E", "BIT 3 H", "BIT 3 L", "BIT 3 (HL)", "BIT 3 A",
"BIT 4 B", "BIT 4 C", "BIT 4 D", "BIT 4 E", "BIT 4 H", "BIT 4 L", "BIT 4 (HL)", "BIT 4 A", "BIT 5 B", "BIT 5 C", "BIT 5 D", "BIT 5 E", "BIT 5 H", "BIT 5 L", "BIT 5 (HL)", "BIT 5 A",
"BIT 6 B", "BIT 6 C", "BIT 6 D", "BIT 6 E", "BIT 6 H", "BIT 6 L", "BIT 6 (HL)", "BIT 6 A", "BIT 7 B", "BIT 7 C", "BIT 7 D", "BIT 7 E", "BIT 7 H", "BIT 7 L", "BIT 7 (HL)", "BIT 7 A",
"RES 0 B", "RES 0 C", "RES 0 D", "RES 0 E", "RES 0 H", "RES 0 L", "RES 0 (HL)", "RES 0 A", "RES 1 B", "RES 1 C", "RES 1 D", "RES 1 E", "RES 1 H", "RES 1 L", "RES 1 (HL)", "RES 1 A",
"RES 2 B", "RES 2 C", "RES 2 D", "RES 2 E", "RES 2 H", "RES 2 L", "RES 2 (HL)", "RES 2 A", "RES 3 B", "RES 3 C", "RES 3 D", "RES 3 E", "RES 3 H", "RES 3 L", "RES 3 (HL)", "RES 3 A",
"RES 4 B", "RES 4 C", "RES 4 D", "RES 4 E", "RES 4 H", "RES 4 L", "RES 4 (HL)", "RES 4 A", "RES 5 B", "RES 5 C", "RES 5 D", "RES 5 E", "RES 5 H", "RES 5 L", "RES 5 (HL)", "RES 5 A",
"RES 6 B", "RES 6 C", "RES 6 D", "RES 6 E", "RES 6 H", "RES 6 L", "RES 6 (HL)", "RES 6 A", "RES 7 B", "RES 7 C", "RES 7 D", "RES 7 E", "RES 7 H", "RES 7 L", "RES 7 (HL)", "RES 7 A",
"SET 0 B", "SET 0 C", "SET 0 D", "SET 0 E", "SET 0 H", "SET 0 L", "SET 0 (HL)", "SET 0 A", "SET 1 B", "SET 1 C", "SET 1 D", "SET 1 E", "SET 1 H", "SET 1 L", "SET 1 (HL)", "SET 1 A",
"SET 2 B", "SET 2 C", "SET 2 D", "SET 2 E", "SET 2 H", "SET 2 L", "SET 2 (HL)", "SET 2 A", "SET 3 B", "SET 3 C", "SET 3 D", "SET 3 E", "SET 3 H", "SET 3 L", "SET 3 (HL)", "SET 3 A",
"SET 4 B", "SET 4 C", "SET 4 D", "SET 4 E", "SET 4 H", "SET 4 L", "SET 4 (HL)", "SET 4 A", "SET 5 B", "SET 5 C", "SET 5 D", "SET 5 E", "SET 5 H", "SET 5 L", "SET 5 (HL)", "SET 5 A",
"SET 6 B", "SET 6 C", "SET 6 D", "SET 6 E", "SET 6 H", "SET 6 L", "SET 6 (HL)", "SET 6 A", "SET 7 B", "SET 7 C", "SET 7 D", "SET 7 E", "SET 7 H", "SET 7 L", "SET 7 (HL)", "SET 7 A" };
//------------------------------
#endif // LOGGING_ENABLED

// Forward declaration
uint8_t (*opcode_table[])(CPU* cpu) = { op_00, op_01, op_02, op_03, op_04, op_05, op_06, op_07, op_08, op_09, op_0a, op_0b, op_0c, op_0d, op_0e, op_0f,
                                            op_10, op_11, op_12, op_13, op_14, op_15, op_16, op_17, op_18, op_19, op_1a, op_1b, op_1c, op_1d, op_1e, op_1f,
                                            op_20, op_21, op_22, op_23, op_24, op_25, op_26, op_27, op_28, op_29, op_2a, op_2b, op_2c, op_2d, op_2e, op_2f,
                                            op_30, op_31, op_32, op_33, op_34, op_35, op_36, op_37, op_38, op_39, op_3a, op_3b, op_3c, op_3d, op_3e, op_3f,
                                            op_40, op_41, op_42, op_43, op_44, op_45, op_46, op_47, op_48, op_49, op_4a, op_4b, op_4c, op_4d, op_4e, op_4f,
                                            op_50, op_51, op_52, op_53, op_54, op_55, op_56, op_57, op_58, op_59, op_5a, op_5b, op_5c, op_5d, op_5e, op_5f,
                                            op_60, op_61, op_62, op_63, op_64, op_65, op_66, op_67, op_68, op_69, op_6a, op_6b, op_6c, op_6d, op_6e, op_6f,
                                            op_70, op_71, op_72, op_73, op_74, op_75, op_76, op_77, op_78, op_79, op_7a, op_7b, op_7c, op_7d, op_7e, op_7f,
                                            op_80, op_81, op_82, op_83, op_84, op_85, op_86, op_87, op_88, op_89, op_8a, op_8b, op_8c, op_8d, op_8e, op_8f,
                                            op_90, op_91, op_92, op_93, op_94, op_95, op_96, op_97, op_98, op_99, op_9a, op_9b, op_9c, op_9d, op_9e, op_9f,
                                            op_a0, op_a1, op_a2, op_a3, op_a4, op_a5, op_a6, op_a7, op_a8, op_a9, op_aa, op_ab, op_ac, op_ad, op_ae, op_af,
                                            op_b0, op_b1, op_b2, op_b3, op_b4, op_b5, op_b6, op_b7, op_b8, op_b9, op_ba, op_bb, op_bc, op_bd, op_be, op_bf,
                                            op_c0, op_c1, op_c2, op_c3, op_c4, op_c5, op_c6, op_c7, op_c8, op_c9, op_ca, op_cb, op_cc, op_cd, op_ce, op_cf,
                                            op_d0, op_d1, op_d2, op_d3, op_d4, op_d5, op_d6, op_d7, op_d8, op_d9, op_da, op_db, op_dc, op_dd, op_de, op_df,
                                            op_e0, op_e1, op_e2, op_e3, op_e4, op_e5, op_e6, op_e7, op_e8, op_e9, op_ea, op_eb, op_ec, op_ed, op_ee, op_ef,
                                            op_f0, op_f1, op_f2, op_f3, op_f4, op_f5, op_f6, op_f7, op_f8, op_f9, op_fa, op_fb, op_fc, op_fd, op_fe, op_ff };

uint8_t (*ex_opcode_table[])(CPU* cpu) = { 	op_cb_00, op_cb_01, op_cb_02, op_cb_03, op_cb_04, op_cb_05, op_cb_06, op_cb_07, op_cb_08, op_cb_09, op_cb_0a, op_cb_0b, op_cb_0c, op_cb_0d, op_cb_0e, op_cb_0f,
                                               op_cb_10, op_cb_11, op_cb_12, op_cb_13, op_cb_14, op_cb_15, op_cb_16, op_cb_17, op_cb_18, op_cb_19, op_cb_1a, op_cb_1b, op_cb_1c, op_cb_1d, op_cb_1e, op_cb_1f,
                                               op_cb_20, op_cb_21, op_cb_22, op_cb_23, op_cb_24, op_cb_25, op_cb_26, op_cb_27, op_cb_28, op_cb_29, op_cb_2a, op_cb_2b, op_cb_2c, op_cb_2d, op_cb_2e, op_cb_2f,
                                               op_cb_30, op_cb_31, op_cb_32, op_cb_33, op_cb_34, op_cb_35, op_cb_36, op_cb_37, op_cb_38, op_cb_39, op_cb_3a, op_cb_3b, op_cb_3c, op_cb_3d, op_cb_3e, op_cb_3f,
                                               op_cb_40, op_cb_41, op_cb_42, op_cb_43, op_cb_44, op_cb_45, op_cb_46, op_cb_47, op_cb_48, op_cb_49, op_cb_4a, op_cb_4b, op_cb_4c, op_cb_4d, op_cb_4e, op_cb_4f,
                                               op_cb_50, op_cb_51, op_cb_52, op_cb_53, op_cb_54, op_cb_55, op_cb_56, op_cb_57, op_cb_58, op_cb_59, op_cb_5a, op_cb_5b, op_cb_5c, op_cb_5d, op_cb_5e, op_cb_5f,
                                               op_cb_60, op_cb_61, op_cb_62, op_cb_63, op_cb_64, op_cb_65, op_cb_66, op_cb_67, op_cb_68, op_cb_69, op_cb_6a, op_cb_6b, op_cb_6c, op_cb_6d, op_cb_6e, op_cb_6f,
                                               op_cb_70, op_cb_71, op_cb_72, op_cb_73, op_cb_74, op_cb_75, op_cb_76, op_cb_77, op_cb_78, op_cb_79, op_cb_7a, op_cb_7b, op_cb_7c, op_cb_7d, op_cb_7e, op_cb_7f,
                                               op_cb_80, op_cb_81, op_cb_82, op_cb_83, op_cb_84, op_cb_85, op_cb_86, op_cb_87, op_cb_88, op_cb_89, op_cb_8a, op_cb_8b, op_cb_8c, op_cb_8d, op_cb_8e, op_cb_8f,
                                               op_cb_90, op_cb_91, op_cb_92, op_cb_93, op_cb_94, op_cb_95, op_cb_96, op_cb_97, op_cb_98, op_cb_99, op_cb_9a, op_cb_9b, op_cb_9c, op_cb_9d, op_cb_9e, op_cb_9f,
                                               op_cb_a0, op_cb_a1, op_cb_a2, op_cb_a3, op_cb_a4, op_cb_a5, op_cb_a6, op_cb_a7, op_cb_a8, op_cb_a9, op_cb_aa, op_cb_ab, op_cb_ac, op_cb_ad, op_cb_ae, op_cb_af,
                                               op_cb_b0, op_cb_b1, op_cb_b2, op_cb_b3, op_cb_b4, op_cb_b5, op_cb_b6, op_cb_b7, op_cb_b8, op_cb_b9, op_cb_ba, op_cb_bb, op_cb_bc, op_cb_bd, op_cb_be, op_cb_bf,
                                               op_cb_c0, op_cb_c1, op_cb_c2, op_cb_c3, op_cb_c4, op_cb_c5, op_cb_c6, op_cb_c7, op_cb_c8, op_cb_c9, op_cb_ca, op_cb_cb, op_cb_cc, op_cb_cd, op_cb_ce, op_cb_cf,
                                               op_cb_d0, op_cb_d1, op_cb_d2, op_cb_d3, op_cb_d4, op_cb_d5, op_cb_d6, op_cb_d7, op_cb_d8, op_cb_d9, op_cb_da, op_cb_db, op_cb_dc, op_cb_dd, op_cb_de, op_cb_df,
                                               op_cb_e0, op_cb_e1, op_cb_e2, op_cb_e3, op_cb_e4, op_cb_e5, op_cb_e6, op_cb_e7, op_cb_e8, op_cb_e9, op_cb_ea, op_cb_eb, op_cb_ec, op_cb_ed, op_cb_ee, op_cb_ef,
                                               op_cb_f0, op_cb_f1, op_cb_f2, op_cb_f3, op_cb_f4, op_cb_f5, op_cb_f6, op_cb_f7, op_cb_f8, op_cb_f9, op_cb_fa, op_cb_fb, op_cb_fc, op_cb_fd, op_cb_fe, op_cb_ff };


inline void push_stack(CPU* cpu, Register reg);
inline uint8_t fetch_byte(CPU* cpu);
//


#ifdef LOGGING_ENABLED
uint8_t LogTickCpu(CPU* cpu, std::string& in_str) {
    auto opcode = fetch_byte(cpu);
    in_str = std::string(opcode_names[opcode]);
    return (*opcode_table[opcode])(cpu);
}
#endif

CPU* InitCpu()
{
    CPU* cpu = (CPU*)GB_Alloc(sizeof(CPU));
    cpu->IME = 0;
    cpu->isStopped = false;
    cpu->imeRequested = false;
    
    cpu->AF.reg16 = 0x01B0;
    cpu->BC.reg16 = 0x0013;
    cpu->DE.reg16 = 0x00D8;
    cpu->HL.reg16 = 0x014D;
    cpu->SP.reg16 = 0xFFFE;
#ifdef BIOS_ENABLE
    cpu->PC.reg16 = 0x0000;
#else
    cpu->PC.reg16 = 0x0100;
#endif

	return cpu;
}

uint8_t TickCpu(CPU* cpu)
{
    auto opcode = fetch_byte(cpu);
    return (*opcode_table[opcode])(cpu);
}

uint8_t HandleInterrupts(CPU* cpu) {
    uint8_t cycles = 0;

    uint8_t IE = GB_Read(0xFFFF);
    uint8_t IF = GB_Read(0xFF0F);
    uint8_t interrupts = IE & IF;

    bool prevIsStopped = cpu->isStopped;

    if (interrupts != 0) {
        cpu->isStopped = false;
        
        if (prevIsStopped == true && cpu->isStopped == false)
            cycles += 4;
    }

    if (cpu->IME) {
        for (int i = 0; i < 5; ++i) {
            if (interrupts & 0x01) {

                cpu->IME = false;
                GB_Write(0xFF0F, BIT_CLEAR(IF, i)); // reset bit in IF

                // 2 M cycles pass 

                // 2 M cycles
                push_stack(cpu, cpu->PC);

                // 1 M cycle
                cpu->PC.reg16 = interruptVectors[i];
                cycles += 20; // 5 M cycles = 20 T cycles;
                break;
            }
            interrupts >>= 1;
        }
    }
    return cycles;
}

inline uint8_t rlc(CPU* cpu, uint8_t num) {
    uint8_t msbit = num >> 7;
    BIT_CLEAR(cpu->AF.lo, F_N);
    BIT_CLEAR(cpu->AF.lo, F_H);
    msbit ? BIT_SET(cpu->AF.lo, F_C) : BIT_CLEAR(cpu->AF.lo, F_C);
    
    uint8_t result = ((num << 1) | msbit);

    (result == 0x00) ? BIT_SET(cpu->AF.lo, F_Z) : BIT_CLEAR(cpu->AF.lo, F_Z);
    return result;
}

inline uint8_t rrc(CPU* cpu, uint8_t num) {
    uint8_t lsbit = num & 0x01;
    BIT_CLEAR(cpu->AF.lo, F_N);
    BIT_CLEAR(cpu->AF.lo, F_H);
    lsbit ? BIT_SET(cpu->AF.lo, F_C) : BIT_CLEAR(cpu->AF.lo, F_C);

    uint8_t result = ((num >> 1) | (lsbit << 7));

    (result == 0x00) ? BIT_SET(cpu->AF.lo, F_Z) : BIT_CLEAR(cpu->AF.lo, F_Z);
    return result;
}


inline uint8_t rl(CPU* cpu, uint8_t num) {
    uint8_t msbit = num >> 7;
    BIT_CLEAR(cpu->AF.lo, F_N);
    BIT_CLEAR(cpu->AF.lo, F_H);
    
    uint8_t carrybit = BIT_GET(cpu->AF.lo, F_C);
    msbit ? BIT_SET(cpu->AF.lo, F_C) : BIT_CLEAR(cpu->AF.lo, F_C);

    uint8_t result = (num << 1) | carrybit;
    (result == 0x00) ? BIT_SET(cpu->AF.lo, F_Z) : BIT_CLEAR(cpu->AF.lo, F_Z);
    return result;
}

inline uint8_t rr(CPU* cpu, uint8_t num) {
    uint8_t lsbit = num & 0x01;
    BIT_CLEAR(cpu->AF.lo, F_N);
    BIT_CLEAR(cpu->AF.lo, F_H);

    uint8_t carrybit = BIT_GET(cpu->AF.lo, F_C);
    lsbit ? BIT_SET(cpu->AF.lo, F_C) : BIT_CLEAR(cpu->AF.lo, F_C);

    uint8_t result = (num >> 1) | (carrybit << 7);
    (result == 0x00) ? BIT_SET(cpu->AF.lo, F_Z) : BIT_CLEAR(cpu->AF.lo, F_Z);
    return result;
}


inline uint8_t sla(CPU* cpu, uint8_t num) {
    uint8_t msbit = num >> 7;
    BIT_CLEAR(cpu->AF.lo, F_N);
    BIT_CLEAR(cpu->AF.lo, F_H);
    msbit ? BIT_SET(cpu->AF.lo, F_C) : BIT_CLEAR(cpu->AF.lo, F_C);

    uint8_t result = num << 1;

    (result == 0x00) ? BIT_SET(cpu->AF.lo, F_Z) : BIT_CLEAR(cpu->AF.lo, F_Z);
    return result;
}

inline uint8_t sra(CPU* cpu, uint8_t num) {
    uint8_t temp = num & 0x80;
    uint8_t lsbit = num & 0x01;
    BIT_CLEAR(cpu->AF.lo, F_N);
    BIT_CLEAR(cpu->AF.lo, F_H);
    lsbit ? BIT_SET(cpu->AF.lo, F_C) : BIT_CLEAR(cpu->AF.lo, F_C);

    uint8_t result = (num >> 1) | temp;

    (result == 0x00) ? BIT_SET(cpu->AF.lo, F_Z) : BIT_CLEAR(cpu->AF.lo, F_Z);
    return result;
}

inline uint8_t srl(CPU* cpu, uint8_t num) {
    uint8_t lsbit = num & 0x01;
    BIT_CLEAR(cpu->AF.lo, F_N);
    BIT_CLEAR(cpu->AF.lo, F_H);
    lsbit ? BIT_SET(cpu->AF.lo, F_C) : BIT_CLEAR(cpu->AF.lo, F_C);

    uint8_t result = (num >> 1);

    (result == 0x00) ? BIT_SET(cpu->AF.lo, F_Z) : BIT_CLEAR(cpu->AF.lo, F_Z);
    return result;
}

    inline uint8_t fetch_byte(CPU* cpu) {
        // Read(PC++)
        return GB_Read(cpu->PC.reg16++);
    }
    inline uint16_t fetch_word(CPU* cpu) {
        // lsb = Read(PC++)
        // msb = Read(PC++)
        // return word(lsb, msb)
        uint8_t lsb = GB_Read(cpu->PC.reg16++);
        uint16_t msb = GB_Read(cpu->PC.reg16++);
        return (msb << 8) | lsb;
    }

    inline void push_stack(CPU* cpu, Register reg) {
        cpu->SP.reg16--;
        GB_Write(cpu->SP.reg16, reg.hi);
        cpu->SP.reg16--;
        GB_Write(cpu->SP.reg16, reg.lo);
    }

    inline uint16_t pop_stack(CPU* cpu) {
        uint8_t lsb = GB_Read(cpu->SP.reg16);
        cpu->SP.reg16++;
        uint16_t msb = GB_Read(cpu->SP.reg16);
        cpu->SP.reg16++;
        return (msb << 8) | lsb;
    }

    inline void test_n_set(CPU* cpu, uint8_t reg, uint8_t bit) {
        if (BIT_GET(reg, bit))
            BIT_CLEAR(cpu->AF.lo, F_Z);
        else
            BIT_SET(cpu->AF.lo, F_Z);
        
        BIT_CLEAR(cpu->AF.lo, F_N);
        BIT_SET(cpu->AF.lo, F_H);
    }

    inline uint8_t swap_n_set(CPU* cpu, uint8_t num) {
        BIT_CLEAR(cpu->AF.lo, F_N);
        BIT_CLEAR(cpu->AF.lo, F_H);
        BIT_CLEAR(cpu->AF.lo, F_C);

        uint8_t result = ((num << 4) | (num >> 4)) & 0xFF;
        if (result == 0)
            BIT_SET(cpu->AF.lo, F_Z);
        else
            BIT_CLEAR(cpu->AF.lo, F_Z);
        return result;
    }

    inline uint8_t inc_r8(CPU* cpu, uint8_t reg8) {
        reg8++;
        (reg8 == 0x00) ? BIT_SET(cpu->AF.lo, F_Z) : BIT_CLEAR(cpu->AF.lo, F_Z);
        BIT_CLEAR(cpu->AF.lo, F_N);
        ((reg8 & 0x0F) == 0x00) ? BIT_SET(cpu->AF.lo, F_H) : BIT_CLEAR(cpu->AF.lo, F_H);
        return reg8;
    }

    inline uint8_t dec_r8(CPU* cpu, uint8_t reg8) {
        reg8--;
        (reg8 == 0x00) ? BIT_SET(cpu->AF.lo, F_Z) : BIT_CLEAR(cpu->AF.lo, F_Z);
        BIT_SET(cpu->AF.lo, F_N);
        ((reg8 & 0x0F) == 0x0F) ? BIT_SET(cpu->AF.lo, F_H) : BIT_CLEAR(cpu->AF.lo, F_H);
        return reg8;
    }

    inline void add_hl(CPU* cpu, uint16_t num) {
        uint32_t sum = cpu->HL.reg16 + num;
        BIT_CLEAR(cpu->AF.lo, F_N);
        (sum ^ cpu->HL.reg16 ^ num) & 0x1000 ? BIT_SET(cpu->AF.lo, F_H) : BIT_CLEAR(cpu->AF.lo, F_H);
        (sum & 0xFFFF0000) ? BIT_SET(cpu->AF.lo, F_C) : BIT_CLEAR(cpu->AF.lo, F_C);
        cpu->HL.reg16 = sum & 0xFFFF;
    }

    inline void add_byte(CPU* cpu, uint8_t num, uint8_t carry) {
        uint16_t sum = (uint16_t)cpu->AF.hi + num + carry;
        uint8_t flagReg = cpu->AF.lo;
        
        (sum & 0xFF) == 0x00 ? BIT_SET(flagReg, F_Z) : BIT_CLEAR(flagReg, F_Z);
        BIT_CLEAR(flagReg, F_N);
        (cpu->AF.hi ^ num ^ sum) & 0x10 ? BIT_SET(flagReg, F_H) : BIT_CLEAR(flagReg, F_H);
        (sum & 0xFF00) ? BIT_SET(flagReg, F_C) : BIT_CLEAR(flagReg, F_C);
        cpu->AF.hi = sum & 0xFF;

        cpu->AF.lo = flagReg;
    }

    inline void sub_byte(CPU* cpu, uint8_t num, uint8_t carry) {
        uint16_t sum = (uint16_t)cpu->AF.hi - num - carry;
        uint8_t flagReg = cpu->AF.lo;

        (sum & 0xFF) == 0x00 ? BIT_SET(flagReg, F_Z) : BIT_CLEAR(flagReg, F_Z);
        BIT_SET(flagReg, F_N);
        (cpu->AF.hi ^ num ^ sum) & 0x10 ? BIT_SET(flagReg, F_H) : BIT_CLEAR(flagReg, F_H);
        (sum & 0xFF00) ? BIT_SET(flagReg, F_C) : BIT_CLEAR(flagReg, F_C);
        cpu->AF.hi = sum & 0xFF;

        cpu->AF.lo = flagReg;
    }

    inline void and_byte(CPU* cpu, uint8_t num) {
        uint8_t flagReg = cpu->AF.lo;

        cpu->AF.hi &= num;
        cpu->AF.hi == 0 ? BIT_SET(flagReg, F_Z) : BIT_CLEAR(flagReg, F_Z);
        BIT_CLEAR(flagReg, F_N);
        BIT_SET(flagReg, F_H);
        BIT_CLEAR(flagReg, F_C);

        cpu->AF.lo = flagReg;
    }

    inline void xor_byte(CPU* cpu, uint8_t num) {
        uint8_t flagReg = cpu->AF.lo;

        cpu->AF.hi ^= num;
        cpu->AF.hi == 0 ? BIT_SET(flagReg, F_Z) : BIT_CLEAR(flagReg, F_Z);
        BIT_CLEAR(flagReg, F_N);
        BIT_CLEAR(flagReg, F_H);
        BIT_CLEAR(flagReg, F_C);

        cpu->AF.lo = flagReg;
    }

    inline void or_byte(CPU* cpu, uint8_t num) {
        uint8_t flagReg = cpu->AF.lo;

        cpu->AF.hi |= num;
        cpu->AF.hi == 0 ? BIT_SET(flagReg, F_Z) : BIT_CLEAR(flagReg, F_Z);
        BIT_CLEAR(flagReg, F_N);
        BIT_CLEAR(flagReg, F_H);
        BIT_CLEAR(flagReg, F_C);

        cpu->AF.lo = flagReg;
    }

    inline void cp_byte(CPU* cpu, uint8_t num) {
        // Almost identical to sub_byte
        // The only difference is that ACC is not updated.
        uint16_t sum = (uint16_t)cpu->AF.hi - num;
        uint8_t flagReg = cpu->AF.lo;

        (sum & 0xFF) == 0x00 ? BIT_SET(flagReg, F_Z) : BIT_CLEAR(flagReg, F_Z);
        BIT_SET(flagReg, F_N);
        (cpu->AF.hi ^ num ^ sum) & 0x10 ? BIT_SET(flagReg, F_H) : BIT_CLEAR(flagReg, F_H);
        (sum & 0xFF00) ? BIT_SET(flagReg, F_C) : BIT_CLEAR(flagReg, F_C);

        cpu->AF.lo = flagReg;
    }

    uint8_t op_cb_40(CPU* cpu) {test_n_set(cpu, cpu->BC.hi, 0); return 8;}
	uint8_t op_cb_41(CPU* cpu) {test_n_set(cpu, cpu->BC.lo, 0); return 8;}
    uint8_t op_cb_42(CPU* cpu) {test_n_set(cpu, cpu->DE.hi, 0); return 8;}
    uint8_t op_cb_43(CPU* cpu) {test_n_set(cpu, cpu->DE.lo, 0); return 8;}
    uint8_t op_cb_44(CPU* cpu) {test_n_set(cpu, cpu->HL.hi, 0); return 8;}
    uint8_t op_cb_45(CPU* cpu) {test_n_set(cpu, cpu->HL.lo, 0); return 8;}
    uint8_t op_cb_46(CPU* cpu) {test_n_set(cpu, GB_Read(cpu->HL.reg16), 0); return 12; }
    uint8_t op_cb_47(CPU* cpu) {test_n_set(cpu, cpu->AF.hi, 0); return 8;}
	             
    uint8_t op_cb_48(CPU* cpu) {test_n_set(cpu, cpu->BC.hi, 1); return 8;}
    uint8_t op_cb_49(CPU* cpu) {test_n_set(cpu, cpu->BC.lo, 1); return 8;}
    uint8_t op_cb_4a(CPU* cpu) {test_n_set(cpu, cpu->DE.hi, 1); return 8;}
    uint8_t op_cb_4b(CPU* cpu) {test_n_set(cpu, cpu->DE.lo, 1); return 8;}
    uint8_t op_cb_4c(CPU* cpu) {test_n_set(cpu, cpu->HL.hi, 1); return 8;}
    uint8_t op_cb_4d(CPU* cpu) {test_n_set(cpu, cpu->HL.lo, 1); return 8;}
    uint8_t op_cb_4e(CPU* cpu) { test_n_set(cpu, GB_Read(cpu->HL.reg16), 1); return 12;}
    uint8_t op_cb_4f(CPU* cpu) { test_n_set(cpu, cpu->AF.hi, 1); return 8;}
                
    uint8_t op_cb_50(CPU* cpu) {test_n_set(cpu, cpu->BC.hi, 2); return 8;}
    uint8_t op_cb_51(CPU* cpu) {test_n_set(cpu, cpu->BC.lo, 2); return 8;}
    uint8_t op_cb_52(CPU* cpu) {test_n_set(cpu, cpu->DE.hi, 2); return 8;}
    uint8_t op_cb_53(CPU* cpu) {test_n_set(cpu, cpu->DE.lo, 2); return 8;}
    uint8_t op_cb_54(CPU* cpu) {test_n_set(cpu, cpu->HL.hi, 2); return 8;}
    uint8_t op_cb_55(CPU* cpu) {test_n_set(cpu, cpu->HL.lo, 2); return 8;}
    uint8_t op_cb_56(CPU* cpu) { test_n_set(cpu, GB_Read(cpu->HL.reg16), 2); return 12;}
    uint8_t op_cb_57(CPU* cpu) { test_n_set(cpu, cpu->AF.hi, 2); return 8;}
                
    uint8_t op_cb_58(CPU* cpu) {test_n_set(cpu, cpu->BC.hi, 3); return 8;}
    uint8_t op_cb_59(CPU* cpu) {test_n_set(cpu, cpu->BC.lo, 3); return 8;}
    uint8_t op_cb_5a(CPU* cpu) {test_n_set(cpu, cpu->DE.hi, 3); return 8;}
    uint8_t op_cb_5b(CPU* cpu) {test_n_set(cpu, cpu->DE.lo, 3); return 8;}
    uint8_t op_cb_5c(CPU* cpu) {test_n_set(cpu, cpu->HL.hi, 3); return 8;}
    uint8_t op_cb_5d(CPU* cpu) {test_n_set(cpu, cpu->HL.lo, 3); return 8;}
    uint8_t op_cb_5e(CPU* cpu) { test_n_set(cpu, GB_Read(cpu->HL.reg16), 3); return 12;}
    uint8_t op_cb_5f(CPU* cpu) { test_n_set(cpu, cpu->AF.hi, 3); return 8;}
              
    uint8_t op_cb_60(CPU* cpu) {test_n_set(cpu, cpu->BC.hi, 4); return 8;}
    uint8_t op_cb_61(CPU* cpu) {test_n_set(cpu, cpu->BC.lo, 4); return 8;}
    uint8_t op_cb_62(CPU* cpu) {test_n_set(cpu, cpu->DE.hi, 4); return 8;}
    uint8_t op_cb_63(CPU* cpu) {test_n_set(cpu, cpu->DE.lo, 4); return 8;}
    uint8_t op_cb_64(CPU* cpu) {test_n_set(cpu, cpu->HL.hi, 4); return 8;}
    uint8_t op_cb_65(CPU* cpu) {test_n_set(cpu, cpu->HL.lo, 4); return 8;}
    uint8_t op_cb_66(CPU* cpu) { test_n_set(cpu, GB_Read(cpu->HL.reg16), 4); return 12;}
    uint8_t op_cb_67(CPU* cpu) {test_n_set(cpu, cpu->AF.hi, 4); return 8;}
               
    uint8_t op_cb_68(CPU* cpu) {test_n_set(cpu, cpu->BC.hi, 5); return 8;}
    uint8_t op_cb_69(CPU* cpu) {test_n_set(cpu, cpu->BC.lo, 5); return 8;}
    uint8_t op_cb_6a(CPU* cpu) {test_n_set(cpu, cpu->DE.hi, 5); return 8;}
    uint8_t op_cb_6b(CPU* cpu) {test_n_set(cpu, cpu->DE.lo, 5); return 8;}
    uint8_t op_cb_6c(CPU* cpu) {test_n_set(cpu, cpu->HL.hi, 5); return 8;}
    uint8_t op_cb_6d(CPU* cpu) {test_n_set(cpu, cpu->HL.lo, 5); return 8;}
    uint8_t op_cb_6e(CPU* cpu) { test_n_set(cpu, GB_Read(cpu->HL.reg16), 5); return 12;}
    uint8_t op_cb_6f(CPU* cpu) {test_n_set(cpu, cpu->AF.hi, 5); return 8;}
             
    uint8_t op_cb_70(CPU* cpu) {test_n_set(cpu, cpu->BC.hi, 6); return 8;}
    uint8_t op_cb_71(CPU* cpu) {test_n_set(cpu, cpu->BC.lo, 6); return 8;}
    uint8_t op_cb_72(CPU* cpu) {test_n_set(cpu, cpu->DE.hi, 6); return 8;}
    uint8_t op_cb_73(CPU* cpu) {test_n_set(cpu, cpu->DE.lo, 6); return 8;}
    uint8_t op_cb_74(CPU* cpu) {test_n_set(cpu, cpu->HL.hi, 6); return 8;}
    uint8_t op_cb_75(CPU* cpu) {test_n_set(cpu, cpu->HL.lo, 6); return 8;}
    uint8_t op_cb_76(CPU* cpu) { test_n_set(cpu, GB_Read(cpu->HL.reg16), 6); return 12;}
    uint8_t op_cb_77(CPU* cpu) {test_n_set(cpu, cpu->AF.hi, 6); return 8;}
             
    uint8_t op_cb_78(CPU* cpu) {test_n_set(cpu, cpu->BC.hi, 7); return 8;}
    uint8_t op_cb_79(CPU* cpu) {test_n_set(cpu, cpu->BC.lo, 7); return 8;}
    uint8_t op_cb_7a(CPU* cpu) {test_n_set(cpu, cpu->DE.hi, 7); return 8;}
    uint8_t op_cb_7b(CPU* cpu) {test_n_set(cpu, cpu->DE.lo, 7); return 8;}
    uint8_t op_cb_7c(CPU* cpu) {test_n_set(cpu, cpu->HL.hi, 7); return 8;}
    uint8_t op_cb_7d(CPU* cpu) {test_n_set(cpu, cpu->HL.lo, 7); return 8;}
    uint8_t op_cb_7e(CPU* cpu) { test_n_set(cpu, GB_Read(cpu->HL.reg16), 7); return 12;}
    uint8_t op_cb_7f(CPU* cpu) {test_n_set(cpu, cpu->AF.hi, 7); return 8;}
                 
    
    uint8_t op_cb_c0(CPU* cpu) {BIT_SET(cpu->BC.hi, 0); return 8; }
    uint8_t op_cb_c1(CPU* cpu) {BIT_SET(cpu->BC.lo, 0); return 8;}
    uint8_t op_cb_c2(CPU* cpu) {BIT_SET(cpu->DE.hi, 0); return 8;}
    uint8_t op_cb_c3(CPU* cpu) {BIT_SET(cpu->DE.lo, 0); return 8;}
    uint8_t op_cb_c4(CPU* cpu) {BIT_SET(cpu->HL.hi, 0); return 8;}
    uint8_t op_cb_c5(CPU* cpu) {BIT_SET(cpu->HL.lo, 0); return 8;}
    uint8_t op_cb_c6(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_SET(num, 0));
        return 16;
    }
    uint8_t op_cb_c7(CPU* cpu) {BIT_SET(cpu->AF.hi, 0); return 8;}
                 
    uint8_t op_cb_c8(CPU* cpu) {BIT_SET(cpu->BC.hi, 1); return 8;}
    uint8_t op_cb_c9(CPU* cpu) {BIT_SET(cpu->BC.lo, 1); return 8;}
    uint8_t op_cb_ca(CPU* cpu) {BIT_SET(cpu->DE.hi, 1); return 8;}
    uint8_t op_cb_cb(CPU* cpu) {BIT_SET(cpu->DE.lo, 1); return 8;}
    uint8_t op_cb_cc(CPU* cpu) {BIT_SET(cpu->HL.hi, 1); return 8;}
    uint8_t op_cb_cd(CPU* cpu) {BIT_SET(cpu->HL.lo, 1); return 8;}
    uint8_t op_cb_ce(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_SET(num, 1));
        return 16;
    }
    uint8_t op_cb_cf(CPU* cpu) {BIT_SET(cpu->AF.hi, 1); return 8;}
             
    uint8_t op_cb_d0(CPU* cpu) {BIT_SET(cpu->BC.hi, 2); return 8;}
    uint8_t op_cb_d1(CPU* cpu) {BIT_SET(cpu->BC.lo, 2); return 8;}
    uint8_t op_cb_d2(CPU* cpu) {BIT_SET(cpu->DE.hi, 2); return 8;}
    uint8_t op_cb_d3(CPU* cpu) {BIT_SET(cpu->DE.lo, 2); return 8;}
    uint8_t op_cb_d4(CPU* cpu) {BIT_SET(cpu->HL.hi, 2); return 8;}
    uint8_t op_cb_d5(CPU* cpu) {BIT_SET(cpu->HL.lo, 2); return 8;}
    uint8_t op_cb_d6(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_SET(num, 2));
        return 16;
    }
    uint8_t op_cb_d7(CPU* cpu) {BIT_SET(cpu->AF.hi, 2); return 8;}
              
    uint8_t op_cb_d8(CPU* cpu) {BIT_SET(cpu->BC.hi, 3); return 8;}
    uint8_t op_cb_d9(CPU* cpu) {BIT_SET(cpu->BC.lo, 3); return 8;}
    uint8_t op_cb_da(CPU* cpu) {BIT_SET(cpu->DE.hi, 3); return 8;}
    uint8_t op_cb_db(CPU* cpu) {BIT_SET(cpu->DE.lo, 3); return 8;}
    uint8_t op_cb_dc(CPU* cpu) {BIT_SET(cpu->HL.hi, 3); return 8;}
    uint8_t op_cb_dd(CPU* cpu) {BIT_SET(cpu->HL.lo, 3); return 8;}
    uint8_t op_cb_de(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_SET(num, 3));
        return 16;
    }
    uint8_t op_cb_df(CPU* cpu) {BIT_SET(cpu->AF.hi, 3); return 8;}
                
    uint8_t op_cb_e0(CPU* cpu) {BIT_SET(cpu->BC.hi, 4); return 8;}
    uint8_t op_cb_e1(CPU* cpu) {BIT_SET(cpu->BC.lo, 4); return 8;}
    uint8_t op_cb_e2(CPU* cpu) {BIT_SET(cpu->DE.hi, 4); return 8;}
    uint8_t op_cb_e3(CPU* cpu) {BIT_SET(cpu->DE.lo, 4); return 8;}
    uint8_t op_cb_e4(CPU* cpu) {BIT_SET(cpu->HL.hi, 4); return 8;}
    uint8_t op_cb_e5(CPU* cpu) {BIT_SET(cpu->HL.lo, 4); return 8;}
    uint8_t op_cb_e6(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_SET(num, 4));
        return 16;
    }
    uint8_t op_cb_e7(CPU* cpu) {BIT_SET(cpu->AF.hi, 4); return 8;}
                            
    uint8_t op_cb_e8(CPU* cpu) {BIT_SET(cpu->BC.hi, 5); return 8;}
    uint8_t op_cb_e9(CPU* cpu) {BIT_SET(cpu->BC.lo, 5); return 8;}
    uint8_t op_cb_ea(CPU* cpu) {BIT_SET(cpu->DE.hi, 5); return 8;}
    uint8_t op_cb_eb(CPU* cpu) {BIT_SET(cpu->DE.lo, 5); return 8;}
    uint8_t op_cb_ec(CPU* cpu) {BIT_SET(cpu->HL.hi, 5); return 8;}
    uint8_t op_cb_ed(CPU* cpu) {BIT_SET(cpu->HL.lo, 5); return 8;}
    uint8_t op_cb_ee(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_SET(num,5 ));
        return 16;
    }
    uint8_t op_cb_ef(CPU* cpu) {BIT_SET(cpu->AF.hi, 5); return 8;}
           
    uint8_t op_cb_f0(CPU* cpu) {BIT_SET(cpu->BC.hi, 6); return 8;}
    uint8_t op_cb_f1(CPU* cpu) {BIT_SET(cpu->BC.lo, 6); return 8;}
    uint8_t op_cb_f2(CPU* cpu) {BIT_SET(cpu->DE.hi, 6); return 8;}
    uint8_t op_cb_f3(CPU* cpu) {BIT_SET(cpu->DE.lo, 6); return 8;}
    uint8_t op_cb_f4(CPU* cpu) {BIT_SET(cpu->HL.hi, 6); return 8;}
    uint8_t op_cb_f5(CPU* cpu) {BIT_SET(cpu->HL.lo, 6); return 8;}
    uint8_t op_cb_f6(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_SET(num, 6));
        return 16;
    }
    uint8_t op_cb_f7(CPU* cpu) {BIT_SET(cpu->AF.hi, 6); return 8;}
                            
    uint8_t op_cb_f8(CPU* cpu) {BIT_SET(cpu->BC.hi, 7); return 8;}
    uint8_t op_cb_f9(CPU* cpu) {BIT_SET(cpu->BC.lo, 7); return 8;}
    uint8_t op_cb_fa(CPU* cpu) {BIT_SET(cpu->DE.hi, 7); return 8;}
    uint8_t op_cb_fb(CPU* cpu) {BIT_SET(cpu->DE.lo, 7); return 8;}
    uint8_t op_cb_fc(CPU* cpu) {BIT_SET(cpu->HL.hi, 7); return 8;}
    uint8_t op_cb_fd(CPU* cpu) {BIT_SET(cpu->HL.lo, 7); return 8;}
    uint8_t op_cb_fe(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_SET(num, 7));
        return 16;
    }
    uint8_t op_cb_ff(CPU* cpu) {BIT_SET(cpu->AF.hi, 7); return 8;}
               
    uint8_t op_cb_80(CPU* cpu) {BIT_CLEAR(cpu->BC.hi, 0); return 8;}
    uint8_t op_cb_81(CPU* cpu) {BIT_CLEAR(cpu->BC.lo, 0); return 8;}
    uint8_t op_cb_82(CPU* cpu) {BIT_CLEAR(cpu->DE.hi, 0); return 8;}
    uint8_t op_cb_83(CPU* cpu) {BIT_CLEAR(cpu->DE.lo, 0); return 8;}
    uint8_t op_cb_84(CPU* cpu) {BIT_CLEAR(cpu->HL.hi, 0); return 8;}
    uint8_t op_cb_85(CPU* cpu) {BIT_CLEAR(cpu->HL.lo, 0); return 8;}
    uint8_t op_cb_86(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_CLEAR(num, 0));
        return 16;
    }
    uint8_t op_cb_87(CPU* cpu) {BIT_CLEAR(cpu->AF.hi,0); return 8;}
                       
    uint8_t op_cb_88(CPU* cpu) {BIT_CLEAR(cpu->BC.hi, 1); return 8;}
    uint8_t op_cb_89(CPU* cpu) {BIT_CLEAR(cpu->BC.lo, 1); return 8;}
    uint8_t op_cb_8a(CPU* cpu) {BIT_CLEAR(cpu->DE.hi, 1); return 8;}
    uint8_t op_cb_8b(CPU* cpu) {BIT_CLEAR(cpu->DE.lo, 1); return 8;}
    uint8_t op_cb_8c(CPU* cpu) {BIT_CLEAR(cpu->HL.hi, 1); return 8;}
    uint8_t op_cb_8d(CPU* cpu) {BIT_CLEAR(cpu->HL.lo, 1); return 8;}
    uint8_t op_cb_8e(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_CLEAR(num, 1));
        return 16;
    }
    uint8_t op_cb_8f(CPU* cpu) {BIT_CLEAR(cpu->AF.hi, 1); return 8;}
                    
    uint8_t op_cb_90(CPU* cpu) {BIT_CLEAR(cpu->BC.hi, 2); return 8;}
    uint8_t op_cb_91(CPU* cpu) {BIT_CLEAR(cpu->BC.lo, 2); return 8;}
    uint8_t op_cb_92(CPU* cpu) {BIT_CLEAR(cpu->DE.hi, 2); return 8;}
    uint8_t op_cb_93(CPU* cpu) {BIT_CLEAR(cpu->DE.lo, 2); return 8;}
    uint8_t op_cb_94(CPU* cpu) {BIT_CLEAR(cpu->HL.hi, 2); return 8;}
    uint8_t op_cb_95(CPU* cpu) {BIT_CLEAR(cpu->HL.lo, 2); return 8;}
    uint8_t op_cb_96(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_CLEAR(num, 2));
        return 16;
    }
    uint8_t op_cb_97(CPU* cpu) {BIT_CLEAR(cpu->AF.hi, 2); return 8;}
                    
    uint8_t op_cb_98(CPU* cpu) {BIT_CLEAR(cpu->BC.hi, 3); return 8;}
    uint8_t op_cb_99(CPU* cpu) {BIT_CLEAR(cpu->BC.lo, 3); return 8;}
    uint8_t op_cb_9a(CPU* cpu) {BIT_CLEAR(cpu->DE.hi, 3); return 8;}
    uint8_t op_cb_9b(CPU* cpu) {BIT_CLEAR(cpu->DE.lo, 3); return 8;}
    uint8_t op_cb_9c(CPU* cpu) {BIT_CLEAR(cpu->HL.hi, 3); return 8;}
    uint8_t op_cb_9d(CPU* cpu) {BIT_CLEAR(cpu->HL.lo, 3); return 8;}
    uint8_t op_cb_9e(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_CLEAR(num, 3));
        return 16;
    }
    uint8_t op_cb_9f(CPU* cpu) {BIT_CLEAR(cpu->AF.hi, 3); return 8;}
                        
    uint8_t op_cb_a0(CPU* cpu) {BIT_CLEAR(cpu->BC.hi, 4); return 8; }
    uint8_t op_cb_a1(CPU* cpu) {BIT_CLEAR(cpu->BC.lo, 4); return 8; }
    uint8_t op_cb_a2(CPU* cpu) {BIT_CLEAR(cpu->DE.hi, 4); return 8; }
    uint8_t op_cb_a3(CPU* cpu) {BIT_CLEAR(cpu->DE.lo, 4); return 8; }
    uint8_t op_cb_a4(CPU* cpu) {BIT_CLEAR(cpu->HL.hi, 4); return 8; }
    uint8_t op_cb_a5(CPU* cpu) {BIT_CLEAR(cpu->HL.lo, 4); return 8; }
    uint8_t op_cb_a6(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_CLEAR(num,4 ));
        return 16;
    }
    uint8_t op_cb_a7(CPU* cpu) {BIT_CLEAR(cpu->AF.hi, 4); return 8; }
                        
    uint8_t op_cb_a8(CPU* cpu) {BIT_CLEAR(cpu->BC.hi, 5); return 8; }
    uint8_t op_cb_a9(CPU* cpu) {BIT_CLEAR(cpu->BC.lo, 5); return 8; }
    uint8_t op_cb_aa(CPU* cpu) {BIT_CLEAR(cpu->DE.hi, 5); return 8; }
    uint8_t op_cb_ab(CPU* cpu) {BIT_CLEAR(cpu->DE.lo, 5); return 8; }
    uint8_t op_cb_ac(CPU* cpu) {BIT_CLEAR(cpu->HL.hi, 5); return 8; }
    uint8_t op_cb_ad(CPU* cpu) {BIT_CLEAR(cpu->HL.lo, 5); return 8; }
    uint8_t op_cb_ae(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_CLEAR(num, 5));
        return 16;
    }
    uint8_t op_cb_af(CPU* cpu) {BIT_CLEAR(cpu->AF.hi, 5); return 8; }
                           
    uint8_t op_cb_b0(CPU* cpu) {BIT_CLEAR(cpu->BC.hi, 6); return 8;}
    uint8_t op_cb_b1(CPU* cpu) {BIT_CLEAR(cpu->BC.lo, 6); return 8;}
    uint8_t op_cb_b2(CPU* cpu) {BIT_CLEAR(cpu->DE.hi, 6); return 8;}
    uint8_t op_cb_b3(CPU* cpu) {BIT_CLEAR(cpu->DE.lo, 6); return 8;}
    uint8_t op_cb_b4(CPU* cpu) {BIT_CLEAR(cpu->HL.hi, 6); return 8;}
    uint8_t op_cb_b5(CPU* cpu) {BIT_CLEAR(cpu->HL.lo, 6); return 8;}
    uint8_t op_cb_b6(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_CLEAR(num, 6));
        return 16;
    }
    uint8_t op_cb_b7(CPU* cpu) {BIT_CLEAR(cpu->AF.hi, 6); return 8;}
                             
    uint8_t op_cb_b8(CPU* cpu) {BIT_CLEAR(cpu->BC.hi, 7); return 8;}
    uint8_t op_cb_b9(CPU* cpu) {BIT_CLEAR(cpu->BC.lo, 7); return 8;}
    uint8_t op_cb_ba(CPU* cpu) {BIT_CLEAR(cpu->DE.hi, 7); return 8;}
    uint8_t op_cb_bb(CPU* cpu) {BIT_CLEAR(cpu->DE.lo, 7); return 8;}
    uint8_t op_cb_bc(CPU* cpu) {BIT_CLEAR(cpu->HL.hi, 7); return 8;}
    uint8_t op_cb_bd(CPU* cpu) {BIT_CLEAR(cpu->HL.lo, 7); return 8;}
    uint8_t op_cb_be(CPU* cpu) {
        uint8_t num = GB_Read(cpu->HL.reg16);
        GB_Write(cpu->HL.reg16, BIT_CLEAR(num, 7));
        return 16;
    }
    uint8_t op_cb_bf(CPU* cpu) {BIT_CLEAR(cpu->AF.hi, 7); return 8;}
                    
    // Swaps
    uint8_t op_cb_30(CPU* cpu) {cpu->BC.hi = swap_n_set(cpu, cpu->BC.hi); return 8;}
    uint8_t op_cb_31(CPU* cpu) {cpu->BC.lo = swap_n_set(cpu, cpu->BC.lo); return 8;}
    uint8_t op_cb_32(CPU* cpu) {cpu->DE.hi = swap_n_set(cpu, cpu->DE.hi); return 8;}
    uint8_t op_cb_33(CPU* cpu) {cpu->DE.lo = swap_n_set(cpu, cpu->DE.lo); return 8;}
    uint8_t op_cb_34(CPU* cpu) {cpu->HL.hi = swap_n_set(cpu, cpu->HL.hi); return 8;}
    uint8_t op_cb_35(CPU* cpu) {cpu->HL.lo = swap_n_set(cpu, cpu->HL.lo); return 8;}
    uint8_t op_cb_36(CPU* cpu) {
        GB_Write(cpu->HL.reg16, swap_n_set(cpu, GB_Read(cpu->HL.reg16)));
        return 16;
    }
    uint8_t op_cb_37(CPU* cpu) {cpu->AF.hi = swap_n_set(cpu, cpu->AF.hi); return 8;}
      

    // RLC B,C,D,E,H,L,(HL),A
    uint8_t op_cb_00(CPU* cpu) {cpu->BC.hi = rlc(cpu, cpu->BC.hi); return 8;}
    uint8_t op_cb_01(CPU* cpu) {cpu->BC.lo = rlc(cpu, cpu->BC.lo); return 8;}
    uint8_t op_cb_02(CPU* cpu) {cpu->DE.hi = rlc(cpu, cpu->DE.hi); return 8;}
    uint8_t op_cb_03(CPU* cpu) {cpu->DE.lo = rlc(cpu, cpu->DE.lo); return 8;}
    uint8_t op_cb_04(CPU* cpu) {cpu->HL.hi = rlc(cpu, cpu->HL.hi); return 8;}
    uint8_t op_cb_05(CPU* cpu) {cpu->HL.lo = rlc(cpu, cpu->HL.lo); return 8;}
    uint8_t op_cb_06(CPU* cpu) {
        uint8_t data = rlc(cpu, GB_Read(cpu->HL.reg16));
        GB_Write(cpu->HL.reg16, data); 
        return 16; 
    }
    uint8_t op_cb_07(CPU* cpu) {cpu->AF.hi = rlc(cpu, cpu->AF.hi); return 8;}
    
    // RRC B,C,D,E,H,L,(HL),A
    uint8_t op_cb_08(CPU* cpu) {cpu->BC.hi = rrc(cpu, cpu->BC.hi); return 8;}
    uint8_t op_cb_09(CPU* cpu) {cpu->BC.lo = rrc(cpu, cpu->BC.lo); return 8;}
    uint8_t op_cb_0a(CPU* cpu) {cpu->DE.hi = rrc(cpu, cpu->DE.hi); return 8;}
    uint8_t op_cb_0b(CPU* cpu) {cpu->DE.lo = rrc(cpu, cpu->DE.lo); return 8;}
    uint8_t op_cb_0c(CPU* cpu) {cpu->HL.hi = rrc(cpu, cpu->HL.hi); return 8;}
    uint8_t op_cb_0d(CPU* cpu) {cpu->HL.lo = rrc(cpu, cpu->HL.lo); return 8;}
    uint8_t op_cb_0e(CPU* cpu) {
        uint8_t data = rrc(cpu, GB_Read(cpu->HL.reg16));
        GB_Write(cpu->HL.reg16, data);
        return 16;
    }
    uint8_t op_cb_0f(CPU* cpu) {cpu->AF.hi = rrc(cpu, cpu->AF.hi); return 8;}
    
    // RL
    uint8_t op_cb_10(CPU* cpu) {cpu->BC.hi = rl(cpu, cpu->BC.hi); return 8;}
    uint8_t op_cb_11(CPU* cpu) {cpu->BC.lo = rl(cpu, cpu->BC.lo); return 8;}
    uint8_t op_cb_12(CPU* cpu) {cpu->DE.hi = rl(cpu, cpu->DE.hi); return 8;}
    uint8_t op_cb_13(CPU* cpu) {cpu->DE.lo = rl(cpu, cpu->DE.lo); return 8;}
    uint8_t op_cb_14(CPU* cpu) {cpu->HL.hi = rl(cpu, cpu->HL.hi); return 8;}
    uint8_t op_cb_15(CPU* cpu) {cpu->HL.lo = rl(cpu, cpu->HL.lo); return 8;}
    uint8_t op_cb_16(CPU* cpu) {
        uint8_t data = rl(cpu, GB_Read(cpu->HL.reg16));
        GB_Write(cpu->HL.reg16, data);
        return 16;
    }
    uint8_t op_cb_17(CPU* cpu) {cpu->AF.hi = rl(cpu, cpu->AF.hi); return 8;}
            
    // RR
    uint8_t op_cb_18(CPU* cpu) { cpu->BC.hi = rr(cpu, cpu->BC.hi); return 8; }
    uint8_t op_cb_19(CPU* cpu) { cpu->BC.lo = rr(cpu, cpu->BC.lo); return 8; }
    uint8_t op_cb_1a(CPU* cpu) { cpu->DE.hi = rr(cpu, cpu->DE.hi); return 8; }
    uint8_t op_cb_1b(CPU* cpu) { cpu->DE.lo = rr(cpu, cpu->DE.lo); return 8; }
    uint8_t op_cb_1c(CPU* cpu) { cpu->HL.hi = rr(cpu, cpu->HL.hi); return 8; }
    uint8_t op_cb_1d(CPU* cpu) { cpu->HL.lo = rr(cpu, cpu->HL.lo); return 8; }
    uint8_t op_cb_1e(CPU* cpu) {
        uint8_t data = rr(cpu, GB_Read(cpu->HL.reg16));
        GB_Write(cpu->HL.reg16, data);
        return 16;
    }
    uint8_t op_cb_1f(CPU* cpu) { cpu->AF.hi = rr(cpu, cpu->AF.hi); return 8; }
                
    // SLA
    uint8_t op_cb_20(CPU* cpu) { cpu->BC.hi = sla(cpu, cpu->BC.hi); return 8; }
    uint8_t op_cb_21(CPU* cpu) { cpu->BC.lo = sla(cpu, cpu->BC.lo); return 8; }
    uint8_t op_cb_22(CPU* cpu) { cpu->DE.hi = sla(cpu, cpu->DE.hi); return 8; }
    uint8_t op_cb_23(CPU* cpu) { cpu->DE.lo = sla(cpu, cpu->DE.lo); return 8; }
    uint8_t op_cb_24(CPU* cpu) { cpu->HL.hi = sla(cpu, cpu->HL.hi); return 8; }
    uint8_t op_cb_25(CPU* cpu) { cpu->HL.lo = sla(cpu, cpu->HL.lo); return 8; }
    uint8_t op_cb_26(CPU* cpu) {
        uint8_t data = sla(cpu, GB_Read(cpu->HL.reg16));
        GB_Write(cpu->HL.reg16, data);
        return 16;
    }
    uint8_t op_cb_27(CPU* cpu) { cpu->AF.hi = sla(cpu, cpu->AF.hi); return 8; }

    // SRA
    uint8_t op_cb_28(CPU* cpu) { cpu->BC.hi = sra(cpu, cpu->BC.hi); return 8; }
    uint8_t op_cb_29(CPU* cpu) { cpu->BC.lo = sra(cpu, cpu->BC.lo); return 8; }
    uint8_t op_cb_2a(CPU* cpu) { cpu->DE.hi = sra(cpu, cpu->DE.hi); return 8; }
    uint8_t op_cb_2b(CPU* cpu) { cpu->DE.lo = sra(cpu, cpu->DE.lo); return 8; }
    uint8_t op_cb_2c(CPU* cpu) { cpu->HL.hi = sra(cpu, cpu->HL.hi); return 8; }
    uint8_t op_cb_2d(CPU* cpu) { cpu->HL.lo = sra(cpu, cpu->HL.lo); return 8; }
    uint8_t op_cb_2e(CPU* cpu) {
        uint8_t data = sra(cpu, GB_Read(cpu->HL.reg16));
        GB_Write(cpu->HL.reg16, data);
        return 16;
    }
    uint8_t op_cb_2f(CPU* cpu) { cpu->AF.hi = sra(cpu, cpu->AF.hi); return 8; }
    
    // SRL
    uint8_t op_cb_38(CPU* cpu) { cpu->BC.hi = srl(cpu, cpu->BC.hi); return 8; }
    uint8_t op_cb_39(CPU* cpu) { cpu->BC.lo = srl(cpu, cpu->BC.lo); return 8; }
    uint8_t op_cb_3a(CPU* cpu) { cpu->DE.hi = srl(cpu, cpu->DE.hi); return 8; }
    uint8_t op_cb_3b(CPU* cpu) { cpu->DE.lo = srl(cpu, cpu->DE.lo); return 8; }
    uint8_t op_cb_3c(CPU* cpu) { cpu->HL.hi = srl(cpu, cpu->HL.hi); return 8; }
    uint8_t op_cb_3d(CPU* cpu) { cpu->HL.lo = srl(cpu, cpu->HL.lo); return 8; }
    uint8_t op_cb_3e(CPU* cpu) {
        uint8_t data = srl(cpu, GB_Read(cpu->HL.reg16));
        GB_Write(cpu->HL.reg16, data);
        return 16;
    }
    uint8_t op_cb_3f(CPU* cpu) { cpu->AF.hi = srl(cpu, cpu->AF.hi); return 8; }




	uint8_t op_00(CPU* cpu) { return 4; }
    uint8_t op_10(CPU* cpu) { /*TODO Implement this.*/ cpu->isStopped = true; return 4; } // STOP

    // LD r16, d16
    uint8_t op_01(CPU* cpu) { cpu->BC.reg16 = fetch_word(cpu); return 12; }
    uint8_t op_11(CPU* cpu) { cpu->DE.reg16 = fetch_word(cpu); return 12; }
    uint8_t op_21(CPU* cpu) { cpu->HL.reg16 = fetch_word(cpu); return 12; }
    uint8_t op_31(CPU* cpu) { cpu->SP.reg16 = fetch_word(cpu); return 12; }

    // LD [r16], A
    uint8_t op_02(CPU* cpu) { GB_Write(cpu->BC.reg16, cpu->AF.hi); return 8; }
    uint8_t op_12(CPU* cpu) { GB_Write(cpu->DE.reg16, cpu->AF.hi); return 8; }
    // LD [HL+], A
    uint8_t op_22(CPU* cpu) { GB_Write(cpu->HL.reg16, cpu->AF.hi); cpu->HL.reg16++; return 8; }
    // LD [HL-], A
    uint8_t op_32(CPU* cpu) { GB_Write(cpu->HL.reg16, cpu->AF.hi); cpu->HL.reg16--; return 8; }

    // LD (a16), sp
    uint8_t op_08(CPU* cpu) {
        uint16_t addr = fetch_word(cpu);
        GB_Write(addr, cpu->SP.lo);
        GB_Write(addr + 1, cpu->SP.hi);
        return 20;
    }

    // INC r16
    uint8_t op_03(CPU* cpu) { cpu->BC.reg16++; return 8; }
    uint8_t op_13(CPU* cpu) { cpu->DE.reg16++; return 8; }
    uint8_t op_23(CPU* cpu) { cpu->HL.reg16++; return 8; }
    uint8_t op_33(CPU* cpu) { cpu->SP.reg16++; return 8; }

    // INC r8 (Modifies flags)
    uint8_t op_04(CPU* cpu) { cpu->BC.hi = inc_r8(cpu, cpu->BC.hi); return 4; }
    uint8_t op_14(CPU* cpu) { cpu->DE.hi = inc_r8(cpu, cpu->DE.hi); return 4; }
    uint8_t op_24(CPU* cpu) { cpu->HL.hi = inc_r8(cpu, cpu->HL.hi); return 4; }
    // INC [HL]
    uint8_t op_34(CPU* cpu) {
        uint8_t data = GB_Read(cpu->HL.reg16);
        data = inc_r8(cpu, data);
        GB_Write(cpu->HL.reg16, data);
        return 12; 
    }

    // DEC r8 (Modifies flags)
    uint8_t op_05(CPU* cpu) { cpu->BC.hi = dec_r8(cpu, cpu->BC.hi); return 4; }
    uint8_t op_15(CPU* cpu) { cpu->DE.hi = dec_r8(cpu, cpu->DE.hi); return 4; }
    uint8_t op_25(CPU* cpu) { cpu->HL.hi = dec_r8(cpu, cpu->HL.hi); return 4; }
    // DEC [HL]
    uint8_t op_35(CPU* cpu) {
        uint8_t data = GB_Read(cpu->HL.reg16);
        data = dec_r8(cpu, data);
        GB_Write(cpu->HL.reg16, data);
        return 12;
    }

    // LD r8, d8
    uint8_t op_06(CPU* cpu) { cpu->BC.hi = fetch_byte(cpu); return 8; }
    uint8_t op_16(CPU* cpu) { cpu->DE.hi = fetch_byte(cpu); return 8; }
    uint8_t op_26(CPU* cpu) { cpu->HL.hi = fetch_byte(cpu); return 8; }
    uint8_t op_36(CPU* cpu) { GB_Write(cpu->HL.reg16, fetch_byte(cpu)); return 12; }


    // ADD HL, BC
    uint8_t op_09(CPU* cpu) { add_hl(cpu, cpu->BC.reg16); return 8; }
    // ADD HL, DE
    uint8_t op_19(CPU* cpu) { add_hl(cpu, cpu->DE.reg16); return 8; }
    // ADD HL, HL
    uint8_t op_29(CPU* cpu) { add_hl(cpu, cpu->HL.reg16); return 8; }
    // ADD HL, SP
    uint8_t op_39(CPU* cpu) { add_hl(cpu, cpu->SP.reg16); return 8; }

    // LD A, [BC]
    uint8_t op_0a(CPU* cpu) { cpu->AF.hi = GB_Read(cpu->BC.reg16); return 8; }
    // LD A, [DE]
    uint8_t op_1a(CPU* cpu) { cpu->AF.hi = GB_Read(cpu->DE.reg16); return 8; }
    // LD A, [HL+]
    uint8_t op_2a(CPU* cpu) { cpu->AF.hi = GB_Read(cpu->HL.reg16++); return 8; }
    // LD A, [HL-]
    uint8_t op_3a(CPU* cpu) { cpu->AF.hi = GB_Read(cpu->HL.reg16--); return 8; }

    // DEC r16
    uint8_t op_0b(CPU* cpu) { cpu->BC.reg16--; return 8; }
    uint8_t op_1b(CPU* cpu) { cpu->DE.reg16--; return 8; }
    uint8_t op_2b(CPU* cpu) { cpu->HL.reg16--; return 8; }
    uint8_t op_3b(CPU* cpu) { cpu->SP.reg16--; return 8; }
                                  
    // INC r8
    uint8_t op_0c(CPU* cpu) { cpu->BC.lo = inc_r8(cpu, cpu->BC.lo); return 4; }
    uint8_t op_1c(CPU* cpu) { cpu->DE.lo = inc_r8(cpu, cpu->DE.lo); return 4; }
    uint8_t op_2c(CPU* cpu) { cpu->HL.lo = inc_r8(cpu, cpu->HL.lo); return 4; }
    uint8_t op_3c(CPU* cpu) { cpu->AF.hi = inc_r8(cpu, cpu->AF.hi); return 4; }

    // DEC r8
    uint8_t op_0d(CPU* cpu) { cpu->BC.lo = dec_r8(cpu, cpu->BC.lo); return 4; }
    uint8_t op_1d(CPU* cpu) { cpu->DE.lo = dec_r8(cpu, cpu->DE.lo); return 4; }
    uint8_t op_2d(CPU* cpu) { cpu->HL.lo = dec_r8(cpu, cpu->HL.lo); return 4; }
    uint8_t op_3d(CPU* cpu) { cpu->AF.hi = dec_r8(cpu, cpu->AF.hi); return 4; }

    // LD r8, d8
    uint8_t op_0e(CPU* cpu) { cpu->BC.lo = fetch_byte(cpu); return 8; }
    uint8_t op_1e(CPU* cpu) { cpu->DE.lo = fetch_byte(cpu); return 8; }
    uint8_t op_2e(CPU* cpu) { cpu->HL.lo = fetch_byte(cpu); return 8; }
    uint8_t op_3e(CPU* cpu) { cpu->AF.hi = fetch_byte(cpu); return 8; }
    
    // LDs 
    uint8_t op_40(CPU* cpu) { return 4; }
    uint8_t op_41(CPU* cpu) { cpu->BC.hi = cpu->BC.lo; return 4; }
    uint8_t op_42(CPU* cpu) { cpu->BC.hi = cpu->DE.hi; return 4; }
    uint8_t op_43(CPU* cpu) { cpu->BC.hi = cpu->DE.lo; return 4; }
    uint8_t op_44(CPU* cpu) { cpu->BC.hi = cpu->HL.hi; return 4; }
    uint8_t op_45(CPU* cpu) { cpu->BC.hi = cpu->HL.lo; return 4; }
    uint8_t op_46(CPU* cpu) { cpu->BC.hi = GB_Read(cpu->HL.reg16); return 8; }
    uint8_t op_47(CPU* cpu) { cpu->BC.hi = cpu->AF.hi; return 4; }
    
    uint8_t op_48(CPU* cpu) { cpu->BC.lo = cpu->BC.hi; return 4; }
    uint8_t op_49(CPU* cpu) { return 4; }
    uint8_t op_4a(CPU* cpu) { cpu->BC.lo = cpu->DE.hi; return 4; }
    uint8_t op_4b(CPU* cpu) { cpu->BC.lo = cpu->DE.lo; return 4; }
    uint8_t op_4c(CPU* cpu) { cpu->BC.lo = cpu->HL.hi; return 4; }
    uint8_t op_4d(CPU* cpu) { cpu->BC.lo = cpu->HL.lo; return 4; }
    uint8_t op_4e(CPU* cpu) { cpu->BC.lo = GB_Read(cpu->HL.reg16); return 8; }
    uint8_t op_4f(CPU* cpu) { cpu->BC.lo = cpu->AF.hi; return 4; }
  
    uint8_t op_50(CPU* cpu) { cpu->DE.hi = cpu->BC.hi; return 4; }
    uint8_t op_51(CPU* cpu) { cpu->DE.hi = cpu->BC.lo; return 4; }
    uint8_t op_52(CPU* cpu) { return 4; }
    uint8_t op_53(CPU* cpu) { cpu->DE.hi = cpu->DE.lo; return 4; }
    uint8_t op_54(CPU* cpu) { cpu->DE.hi = cpu->HL.hi; return 4; }
    uint8_t op_55(CPU* cpu) { cpu->DE.hi = cpu->HL.lo; return 4; }
    uint8_t op_56(CPU* cpu) { cpu->DE.hi = GB_Read(cpu->HL.reg16); return 8; }
    uint8_t op_57(CPU* cpu) { cpu->DE.hi = cpu->AF.hi; return 4; }
      
    uint8_t op_58(CPU* cpu) { cpu->DE.lo = cpu->BC.hi; return 4; }
    uint8_t op_59(CPU* cpu) { cpu->DE.lo = cpu->BC.lo; return 4; }
    uint8_t op_5a(CPU* cpu) { cpu->DE.lo = cpu->DE.hi; return 4; }
    uint8_t op_5b(CPU* cpu) { return 4; }
    uint8_t op_5c(CPU* cpu) { cpu->DE.lo = cpu->HL.hi; return 4; }
    uint8_t op_5d(CPU* cpu) { cpu->DE.lo = cpu->HL.lo; return 4; }
    uint8_t op_5e(CPU* cpu) { cpu->DE.lo = GB_Read(cpu->HL.reg16); return 8; }
    uint8_t op_5f(CPU* cpu) { cpu->DE.lo = cpu->AF.hi; return 4; }

    uint8_t op_60(CPU* cpu) { cpu->HL.hi = cpu->BC.hi; return 4; }
    uint8_t op_61(CPU* cpu) { cpu->HL.hi = cpu->BC.lo; return 4; }
    uint8_t op_62(CPU* cpu) { cpu->HL.hi = cpu->DE.hi; return 4; }
    uint8_t op_63(CPU* cpu) { cpu->HL.hi = cpu->DE.lo; return 4; }
    uint8_t op_64(CPU* cpu) { return 4; }
    uint8_t op_65(CPU* cpu) { cpu->HL.hi = cpu->HL.lo; return 4; }
    uint8_t op_66(CPU* cpu) { cpu->HL.hi = GB_Read(cpu->HL.reg16); return 8; }
    uint8_t op_67(CPU* cpu) { cpu->HL.hi = cpu->AF.hi; return 4; }
           
    uint8_t op_68(CPU* cpu) { cpu->HL.lo = cpu->BC.hi; return 4; }
    uint8_t op_69(CPU* cpu) { cpu->HL.lo = cpu->BC.lo; return 4; }
    uint8_t op_6a(CPU* cpu) { cpu->HL.lo = cpu->DE.hi; return 4; }
    uint8_t op_6b(CPU* cpu) { cpu->HL.lo = cpu->DE.lo; return 4; }
    uint8_t op_6c(CPU* cpu) { cpu->HL.lo = cpu->HL.hi; return 4; }
    uint8_t op_6d(CPU* cpu) { cpu->HL.lo = cpu->HL.lo; return 4; }
    uint8_t op_6e(CPU* cpu) { cpu->HL.lo = GB_Read(cpu->HL.reg16); return 8; }
    uint8_t op_6f(CPU* cpu) { cpu->HL.lo = cpu->AF.hi; return 4; }

    uint8_t op_70(CPU* cpu) { GB_Write(cpu->HL.reg16, cpu->BC.hi); return 8; }
    uint8_t op_71(CPU* cpu) { GB_Write(cpu->HL.reg16, cpu->BC.lo); return 8; }
    uint8_t op_72(CPU* cpu) { GB_Write(cpu->HL.reg16, cpu->DE.hi); return 8; }
    uint8_t op_73(CPU* cpu) { GB_Write(cpu->HL.reg16, cpu->DE.lo); return 8; }
    uint8_t op_74(CPU* cpu) { GB_Write(cpu->HL.reg16, cpu->HL.hi); return 8; }
    uint8_t op_75(CPU* cpu) { GB_Write(cpu->HL.reg16, cpu->HL.lo); return 8; }
    uint8_t op_76(CPU* cpu) { cpu->isStopped = true; return 4; }
    uint8_t op_77(CPU* cpu) { GB_Write(cpu->HL.reg16, cpu->AF.hi); return 8; }
    
    uint8_t op_78(CPU* cpu) { cpu->AF.hi = cpu->BC.hi; return 4; }
    uint8_t op_79(CPU* cpu) { cpu->AF.hi = cpu->BC.lo; return 4; }
    uint8_t op_7a(CPU* cpu) { cpu->AF.hi = cpu->DE.hi; return 4; }
    uint8_t op_7b(CPU* cpu) { cpu->AF.hi = cpu->DE.lo; return 4; }
    uint8_t op_7c(CPU* cpu) { cpu->AF.hi = cpu->HL.hi; return 4; }
    uint8_t op_7d(CPU* cpu) { cpu->AF.hi = cpu->HL.lo; return 4; }
    uint8_t op_7e(CPU* cpu) { cpu->AF.hi = GB_Read(cpu->HL.reg16); return 8; }
    uint8_t op_7f(CPU* cpu) { cpu->AF.hi = cpu->AF.hi; return 4; }

    // ADD A, r8
    uint8_t op_80(CPU* cpu) { add_byte(cpu, cpu->BC.hi, 0); return 4; }
    uint8_t op_81(CPU* cpu) { add_byte(cpu, cpu->BC.lo, 0); return 4; }
    uint8_t op_82(CPU* cpu) { add_byte(cpu, cpu->DE.hi, 0); return 4; }
    uint8_t op_83(CPU* cpu) { add_byte(cpu, cpu->DE.lo, 0); return 4; }
    uint8_t op_84(CPU* cpu) { add_byte(cpu, cpu->HL.hi, 0); return 4; }
    uint8_t op_85(CPU* cpu) { add_byte(cpu, cpu->HL.lo, 0); return 4; }
    uint8_t op_86(CPU* cpu) { add_byte(cpu, GB_Read(cpu->HL.reg16), 0); return 8; }
    uint8_t op_87(CPU* cpu) { add_byte(cpu, cpu->AF.hi, 0); return 4; }

    // ADC
    uint8_t op_88(CPU* cpu) { add_byte(cpu, cpu->BC.hi, BIT_GET(cpu->AF.lo, F_C)); return 4; }
    uint8_t op_89(CPU* cpu) { add_byte(cpu, cpu->BC.lo, BIT_GET(cpu->AF.lo, F_C)); return 4; }
    uint8_t op_8a(CPU* cpu) { add_byte(cpu, cpu->DE.hi, BIT_GET(cpu->AF.lo, F_C)); return 4; }
    uint8_t op_8b(CPU* cpu) { add_byte(cpu, cpu->DE.lo, BIT_GET(cpu->AF.lo, F_C)); return 4; }
    uint8_t op_8c(CPU* cpu) { add_byte(cpu, cpu->HL.hi, BIT_GET(cpu->AF.lo, F_C)); return 4; }
    uint8_t op_8d(CPU* cpu) { add_byte(cpu, cpu->HL.lo, BIT_GET(cpu->AF.lo, F_C)); return 4; }
    uint8_t op_8e(CPU* cpu) { add_byte(cpu, GB_Read(cpu->HL.reg16), BIT_GET(cpu->AF.lo, F_C)); return 8; }
    uint8_t op_8f(CPU* cpu) { add_byte(cpu, cpu->AF.hi, BIT_GET(cpu->AF.lo, F_C)); return 4; }
    uint8_t op_ce(CPU* cpu) { add_byte(cpu, fetch_byte(cpu), BIT_GET(cpu->AF.lo, F_C)); return 8; }

    // SUB
    uint8_t op_90(CPU* cpu) {sub_byte(cpu, cpu->BC.hi, 0); return 4;}
    uint8_t op_91(CPU* cpu) {sub_byte(cpu, cpu->BC.lo, 0); return 4;}
    uint8_t op_92(CPU* cpu) {sub_byte(cpu, cpu->DE.hi, 0); return 4;}
    uint8_t op_93(CPU* cpu) {sub_byte(cpu, cpu->DE.lo, 0); return 4;}
    uint8_t op_94(CPU* cpu) {sub_byte(cpu, cpu->HL.hi, 0); return 4;}
    uint8_t op_95(CPU* cpu) {sub_byte(cpu, cpu->HL.lo, 0); return 4;}
    uint8_t op_96(CPU* cpu) {sub_byte(cpu, GB_Read(cpu->HL.reg16), 0); return 8; }
    uint8_t op_97(CPU* cpu) {sub_byte(cpu, cpu->AF.hi, 0); return 4;}

    // SBC
    uint8_t op_98(CPU* cpu) {sub_byte(cpu, cpu->BC.hi, BIT_GET(cpu->AF.lo, F_C)); return 4;}
    uint8_t op_99(CPU* cpu) {sub_byte(cpu, cpu->BC.lo, BIT_GET(cpu->AF.lo, F_C)); return 4;}
    uint8_t op_9a(CPU* cpu) {sub_byte(cpu, cpu->DE.hi, BIT_GET(cpu->AF.lo, F_C)); return 4;}
    uint8_t op_9b(CPU* cpu) {sub_byte(cpu, cpu->DE.lo, BIT_GET(cpu->AF.lo, F_C)); return 4;}
    uint8_t op_9c(CPU* cpu) {sub_byte(cpu, cpu->HL.hi, BIT_GET(cpu->AF.lo, F_C)); return 4;}
    uint8_t op_9d(CPU* cpu) {sub_byte(cpu, cpu->HL.lo, BIT_GET(cpu->AF.lo, F_C)); return 4;}
    uint8_t op_9e(CPU* cpu) {sub_byte(cpu, GB_Read(cpu->HL.reg16), BIT_GET(cpu->AF.lo, F_C)); return 8; }
    uint8_t op_9f(CPU* cpu) {sub_byte(cpu, cpu->AF.hi, BIT_GET(cpu->AF.lo, F_C)); return 4;}
    uint8_t op_de(CPU* cpu) {sub_byte(cpu, fetch_byte(cpu), BIT_GET(cpu->AF.lo, F_C)); return 8; }

    // AND
    uint8_t op_a0(CPU* cpu) { and_byte(cpu, cpu->BC.hi); return 4; }
    uint8_t op_a1(CPU* cpu) { and_byte(cpu, cpu->BC.lo); return 4; }
    uint8_t op_a2(CPU* cpu) { and_byte(cpu, cpu->DE.hi); return 4; }
    uint8_t op_a3(CPU* cpu) { and_byte(cpu, cpu->DE.lo); return 4; }
    uint8_t op_a4(CPU* cpu) { and_byte(cpu, cpu->HL.hi); return 4; }
    uint8_t op_a5(CPU* cpu) { and_byte(cpu, cpu->HL.lo); return 4; }
    uint8_t op_a6(CPU* cpu) { and_byte(cpu, GB_Read(cpu->HL.reg16)); return 8; }
    uint8_t op_a7(CPU* cpu) { and_byte(cpu, cpu->AF.hi); return 4; }

    // XOR
    uint8_t op_a8(CPU* cpu) { xor_byte(cpu, cpu->BC.hi); return 4; }
    uint8_t op_a9(CPU* cpu) { xor_byte(cpu, cpu->BC.lo); return 4; }
    uint8_t op_aa(CPU* cpu) { xor_byte(cpu, cpu->DE.hi); return 4; }
    uint8_t op_ab(CPU* cpu) { xor_byte(cpu, cpu->DE.lo); return 4; }
    uint8_t op_ac(CPU* cpu) { xor_byte(cpu, cpu->HL.hi); return 4; }
    uint8_t op_ad(CPU* cpu) { xor_byte(cpu, cpu->HL.lo); return 4; }
    uint8_t op_ae(CPU* cpu) { xor_byte(cpu, GB_Read(cpu->HL.reg16)); return 8; }
    uint8_t op_af(CPU* cpu) { xor_byte(cpu, cpu->AF.hi); return 4; }
    uint8_t op_ee(CPU* cpu) { xor_byte(cpu, fetch_byte(cpu)); return 8; }

    // OR 
    uint8_t op_b0(CPU* cpu) { or_byte(cpu, cpu->BC.hi); return 4; }
    uint8_t op_b1(CPU* cpu) { or_byte(cpu, cpu->BC.lo); return 4; }
    uint8_t op_b2(CPU* cpu) { or_byte(cpu, cpu->DE.hi); return 4; }
    uint8_t op_b3(CPU* cpu) { or_byte(cpu, cpu->DE.lo); return 4; }
    uint8_t op_b4(CPU* cpu) { or_byte(cpu, cpu->HL.hi); return 4; }
    uint8_t op_b5(CPU* cpu) { or_byte(cpu, cpu->HL.lo); return 4; }
    uint8_t op_b6(CPU* cpu) { or_byte(cpu, GB_Read(cpu->HL.reg16)); return 8; }
    uint8_t op_b7(CPU* cpu) { or_byte(cpu, cpu->AF.hi); return 4; }

    // CP
    uint8_t op_b8(CPU* cpu) { cp_byte(cpu, cpu->BC.hi); return 4; }
    uint8_t op_b9(CPU* cpu) { cp_byte(cpu, cpu->BC.lo); return 4; }
    uint8_t op_ba(CPU* cpu) { cp_byte(cpu, cpu->DE.hi); return 4; }
    uint8_t op_bb(CPU* cpu) { cp_byte(cpu, cpu->DE.lo); return 4; }
    uint8_t op_bc(CPU* cpu) { cp_byte(cpu, cpu->HL.hi); return 4; }
    uint8_t op_bd(CPU* cpu) { cp_byte(cpu, cpu->HL.lo); return 4; }
    uint8_t op_be(CPU* cpu) { cp_byte(cpu, GB_Read(cpu->HL.reg16)); return 8; }
    uint8_t op_bf(CPU* cpu) { cp_byte(cpu, cpu->AF.hi); return 4; }
    uint8_t op_fe(CPU* cpu) { cp_byte(cpu, fetch_byte(cpu)); return 8; }

    // RET NZ
    uint8_t op_c0(CPU* cpu) {
        if (BIT_GET(cpu->AF.lo, F_Z) == 0) {
            cpu->PC.reg16 = pop_stack(cpu);
            return 20;
        }
        return 8;
    }
    // RET NC
    uint8_t op_d0(CPU* cpu) {
        if (BIT_GET(cpu->AF.lo, F_C) == 0) {
            cpu->PC.reg16 = pop_stack(cpu);
            return 20;
        }
        return 8;
    }
    // RET Z
    uint8_t op_c8(CPU* cpu) {
        if (BIT_GET(cpu->AF.lo, F_Z) == 1) {
            cpu->PC.reg16 = pop_stack(cpu);
            return 20;
        }
        return 8;
    }
    // RET C
    uint8_t op_d8(CPU* cpu) {
        if (BIT_GET(cpu->AF.lo, F_C) == 1) {
            cpu->PC.reg16 = pop_stack(cpu);
            return 20;
        }
        return 8;
    }
    // RET 
    uint8_t op_c9(CPU* cpu) {
        cpu->PC.reg16 = pop_stack(cpu);
        return 16;
    }
    // RETI
    uint8_t op_d9(CPU* cpu) {
        cpu->PC.reg16 = pop_stack(cpu);
        cpu->IME = 1;
        return 16;
    }


    // POP r16
    uint8_t op_c1(CPU* cpu) { cpu->BC.reg16 = pop_stack(cpu); return 12; }
    uint8_t op_d1(CPU* cpu) { cpu->DE.reg16 = pop_stack(cpu); return 12; }
    uint8_t op_e1(CPU* cpu) { cpu->HL.reg16 = pop_stack(cpu); return 12; }
    uint8_t op_f1(CPU* cpu) { 
        cpu->AF.reg16 = pop_stack(cpu);
        cpu->AF.lo &= 0xF0;
        return 12; }


    // jp a16
    uint8_t op_c3(CPU* cpu) { cpu->PC.reg16 = fetch_word(cpu); return 16; }
    // jp hl
    uint8_t op_e9(CPU* cpu) { cpu->PC.reg16 = cpu->HL.reg16; return 4; }

    // jp nz, a16
    uint8_t op_c2(CPU* cpu) {
        uint16_t addr = fetch_word(cpu);
        if (BIT_GET(cpu->AF.lo, F_Z) == 0) {
            cpu->PC.reg16 = addr;
            return 16;
        }
        return 12;
    }
    // jp z, a16
    uint8_t op_ca(CPU* cpu) {
        uint16_t addr = fetch_word(cpu);
        if (BIT_GET(cpu->AF.lo, F_Z) == 1) {
            cpu->PC.reg16 = addr;
            return 16;
        }
        return 12;
    }
    // jp nc, a16
    uint8_t op_d2(CPU* cpu) {
        uint16_t addr = fetch_word(cpu);
        if (BIT_GET(cpu->AF.lo, F_C) == 0) {
            cpu->PC.reg16 = addr;
            return 16;
        }
        return 12;
    }
    // jp c, a16
    uint8_t op_da(CPU* cpu) {
        uint16_t addr = fetch_word(cpu);
        if (BIT_GET(cpu->AF.lo, F_C) == 1) {
            cpu->PC.reg16 = addr;
            return 16;
        }
        return 12;
    }
    // jp r8
    uint8_t op_18(CPU* cpu) {
        int8_t r8 = (int8_t)fetch_byte(cpu);
        cpu->PC.reg16 += r8;
        return 12;
    }
    // jr nz, r8
    uint8_t op_20(CPU* cpu) {
        int8_t r8 = (int8_t)fetch_byte(cpu);
        if (BIT_GET(cpu->AF.lo, F_Z) == 0) {
            cpu->PC.reg16 += r8;
            return 12;
        }
        return 8;
    }
    // jr z, r8
    uint8_t op_28(CPU* cpu) {
        int8_t r8 = (int8_t)fetch_byte(cpu);
        if (BIT_GET(cpu->AF.lo, F_Z) == 1) {
            cpu->PC.reg16 += r8;
            return 12;
        }
        return 8;
    }
    // jr nc, r8
    uint8_t op_30(CPU* cpu) {
        int8_t r8 = (int8_t)fetch_byte(cpu);
        if (BIT_GET(cpu->AF.lo, F_C) == 0) {
            cpu->PC.reg16 += r8;
            return 12;
        }
        return 8;
    }
    // jr c, r8
    uint8_t op_38(CPU* cpu) {
        int8_t r8 = (int8_t)fetch_byte(cpu);
        if (BIT_GET(cpu->AF.lo, F_C) == 1) {
            cpu->PC.reg16 += r8;
            return 12;
        }
        return 8;
    }

    // call a16
    uint8_t op_cd(CPU* cpu) {
        uint16_t addr = fetch_word(cpu);
        push_stack(cpu, cpu->PC); // Changed the order of these two instructions.
        cpu->PC.reg16 = addr;
        return 24;
    }
    // call nz, a16
    uint8_t op_c4(CPU* cpu) {
        uint16_t addr = fetch_word(cpu);
        if (BIT_GET(cpu->AF.lo, F_Z) == 0) {
            push_stack(cpu, cpu->PC);
            cpu->PC.reg16 = addr;
            return 24;
        }
        return 12;
    }
    // call z, a16
    uint8_t op_cc(CPU* cpu) {
        uint16_t addr = fetch_word(cpu);
        if (BIT_GET(cpu->AF.lo, F_Z) == 1) {
            push_stack(cpu, cpu->PC);
            cpu->PC.reg16 = addr;
            return 24;
        }
        return 12;
    }
    // call nc, a16
    uint8_t op_d4(CPU* cpu) {
        uint16_t addr = fetch_word(cpu);
        if (BIT_GET(cpu->AF.lo, F_C) == 0) {
            push_stack(cpu, cpu->PC);
            cpu->PC.reg16 = addr;
            return 24;
        }
        return 12;
    }
    // call c, a16
    uint8_t op_dc(CPU* cpu) {
        uint16_t addr = fetch_word(cpu);
        if (BIT_GET(cpu->AF.lo, F_C) == 1) {
            push_stack(cpu, cpu->PC);
            cpu->PC.reg16 = addr;
            return 24;
        }
        return 12;
    }


    // PUSH r16
    uint8_t op_c5(CPU* cpu) { push_stack(cpu, cpu->BC); return 16; }
    uint8_t op_d5(CPU* cpu) { push_stack(cpu, cpu->DE); return 16; }
    uint8_t op_e5(CPU* cpu) { push_stack(cpu, cpu->HL); return 16; }
    uint8_t op_f5(CPU* cpu) { push_stack(cpu, cpu->AF); return 16; }

    // ADD A, d8 (imm)
    uint8_t op_c6(CPU* cpu) {add_byte(cpu, fetch_byte(cpu), 0); return 8; }
    // SUB A, d8 (imm)
    uint8_t op_d6(CPU* cpu) {sub_byte(cpu, fetch_byte(cpu), 0); return 8;}
    // AND A, d8 (imm)
    uint8_t op_e6(CPU* cpu) {and_byte(cpu, fetch_byte(cpu)); return 8;}
    // OR A, d8 (imm)
    uint8_t op_f6(CPU* cpu) {or_byte(cpu, fetch_byte(cpu)); return 8;}

    // RST 
    uint8_t op_c7(CPU* cpu) { push_stack(cpu, cpu->PC); cpu->PC.reg16 = 0x0000; return 16; }
    uint8_t op_d7(CPU* cpu) { push_stack(cpu, cpu->PC); cpu->PC.reg16 = 0x0010; return 16; }
    uint8_t op_e7(CPU* cpu) { push_stack(cpu, cpu->PC); cpu->PC.reg16 = 0x0020; return 16; }
    uint8_t op_f7(CPU* cpu) { push_stack(cpu, cpu->PC); cpu->PC.reg16 = 0x0030; return 16; }

    // RST
    uint8_t op_cf(CPU* cpu) { push_stack(cpu, cpu->PC); cpu->PC.reg16 = 0x0008; return 16; }
    uint8_t op_df(CPU* cpu) { push_stack(cpu, cpu->PC); cpu->PC.reg16 = 0x0018; return 16; }
    uint8_t op_ef(CPU* cpu) { push_stack(cpu, cpu->PC); cpu->PC.reg16 = 0x0028; return 16; }
    uint8_t op_ff(CPU* cpu) { push_stack(cpu, cpu->PC); cpu->PC.reg16 = 0x0038; return 16; }

    uint8_t op_cb(CPU* cpu) {
        uint8_t cbCycles = (*ex_opcode_table[fetch_byte(cpu)])(cpu);
        return 4 + cbCycles; }
    uint8_t op_fb(CPU* cpu) { /*cpu->IME = 1;*/ cpu->imeRequested = true; return 4; } // EI
    uint8_t op_f3(CPU* cpu) { cpu->IME = 0; return 4; } // DI


    // LDH [a8], A
    uint8_t op_e0(CPU* cpu) {
        uint8_t lo = fetch_byte(cpu);
        uint16_t hi = 0xFF00;
        GB_Write((hi | lo), cpu->AF.hi);
        return 12;
    }
    // LDH A, [a8]
    uint8_t op_f0(CPU* cpu) {
        uint8_t lo = fetch_byte(cpu);
        uint16_t hi = 0xFF00;
        cpu->AF.hi = GB_Read((hi | lo));
        return 12;
    }
    // LD [C], A
    uint8_t op_e2(CPU* cpu) {
        uint16_t hi = 0xFF00;
        GB_Write((hi | cpu->BC.lo), cpu->AF.hi);
        return 8;
    }
    // LD A, [C]
    uint8_t op_f2(CPU* cpu) {
        uint16_t hi = 0xFF00;
        cpu->AF.hi = GB_Read((hi | cpu->BC.lo));
        return 8;
    }
    // LD SP, HL
    uint8_t op_f9(CPU* cpu) { cpu->SP.reg16 = cpu->HL.reg16; return 8; }
    
    // LD [a16], A
    uint8_t op_ea(CPU* cpu) {
        uint16_t addr = fetch_word(cpu);
        GB_Write(addr, cpu->AF.hi);
        return 16;
    }
    // LD A, [a16] 
    uint8_t op_fa(CPU* cpu) {
        cpu->AF.hi = GB_Read(fetch_word(cpu));
        return 16;
    }

    // ADD SP, e8
    // TODO CHECK THIS
    uint8_t op_e8(CPU* cpu){
        int8_t e8 = (int8_t)fetch_byte(cpu);

        BIT_CLEAR(cpu->AF.lo, F_N);
        BIT_CLEAR(cpu->AF.lo, F_Z);

        uint32_t sum = cpu->SP.reg16 + e8;
        uint32_t carryBits = sum ^ e8 ^ cpu->SP.reg16;

        BIT_GET(carryBits, 4) ? BIT_SET(cpu->AF.lo, F_H) : BIT_CLEAR(cpu->AF.lo, F_H);
        BIT_GET(carryBits, 8) ? BIT_SET(cpu->AF.lo, F_C) : BIT_CLEAR(cpu->AF.lo, F_C);

        cpu->SP.reg16 = sum & 0xFFFF;
        return 16;
    }
    // LD HL, SP + e8
    uint8_t op_f8(CPU* cpu){
        int8_t e8 = (int8_t)fetch_byte(cpu);

        BIT_CLEAR(cpu->AF.lo, F_N);
        BIT_CLEAR(cpu->AF.lo, F_Z);

        uint32_t sum = cpu->SP.reg16 + e8;
        uint32_t carryBits = sum ^ e8 ^ cpu->SP.reg16;

        BIT_GET(carryBits, 4) ? BIT_SET(cpu->AF.lo, F_H) : BIT_CLEAR(cpu->AF.lo, F_H);
        BIT_GET(carryBits, 8) ? BIT_SET(cpu->AF.lo, F_C) : BIT_CLEAR(cpu->AF.lo, F_C);

        cpu->HL.reg16 = sum & 0xFFFF;
        return 12;
    }

    // rlca
    uint8_t op_07(CPU* cpu) { 
        cpu->AF.hi = rlc(cpu, cpu->AF.hi);
        BIT_CLEAR(cpu->AF.lo, F_Z);
        return 4;
    }
    // rla
    uint8_t op_17(CPU* cpu) {
        cpu->AF.hi = rl(cpu, cpu->AF.hi);
        BIT_CLEAR(cpu->AF.lo, F_Z);
        return 4;
    }
    // rrca
    uint8_t op_0f(CPU* cpu) {
        cpu->AF.hi = rrc(cpu, cpu->AF.hi);
        BIT_CLEAR(cpu->AF.lo, F_Z);
        return 4;
    }
    // rra
    uint8_t op_1f(CPU* cpu) {
        cpu->AF.hi = rr(cpu, cpu->AF.hi);
        BIT_CLEAR(cpu->AF.lo, F_Z);
        return 4;
    }

    // DAA
    uint8_t op_27(CPU* cpu) {
        // (I don't know how this works.)

        uint32_t a = cpu->AF.hi;
        uint8_t n = BIT_GET(cpu->AF.lo, F_N);

        if (n == 0) {
            if ((BIT_GET(cpu->AF.lo, F_H) == 1) || ((a & 0x0F) > 0x09)) {
                a += 0x06;
            }
            if ((BIT_GET(cpu->AF.lo, F_C) == 1) || (a > 0x9F)) {
                a += 0x60;
                BIT_SET(cpu->AF.lo, F_C);
            }
        }
        else {
            if (BIT_GET(cpu->AF.lo, F_H) == 1)
                a -= 0x06;
            if (BIT_GET(cpu->AF.lo, F_C) == 1)
                a -= 0x60;
        }

        BIT_CLEAR(cpu->AF.lo, F_H);

        ((a & 0xFF) == 0x00) ? BIT_SET(cpu->AF.lo, F_Z) : BIT_CLEAR(cpu->AF.lo, F_Z);

        cpu->AF.hi = a & 0xFF;
        return 4;
    }
    // SCF
    uint8_t op_37(CPU* cpu) {
        BIT_CLEAR(cpu->AF.lo, F_N);
        BIT_CLEAR(cpu->AF.lo, F_H);
        BIT_SET(cpu->AF.lo, F_C);
        return 4;
    }

    // CPL
    uint8_t op_2f(CPU* cpu) {
        cpu->AF.hi ^= 0xFF;
        BIT_SET(cpu->AF.lo, F_N);
        BIT_SET(cpu->AF.lo, F_H);
        return 4;
    }
    // CCF
    uint8_t op_3f(CPU* cpu) {
        BIT_CLEAR(cpu->AF.lo, F_N);
        BIT_CLEAR(cpu->AF.lo, F_H);
        
        uint8_t c = BIT_GET(cpu->AF.lo, F_C) ^ 0x01;
        c ? BIT_SET(cpu->AF.lo, F_C) : BIT_CLEAR(cpu->AF.lo, F_C);
        return 4;
    }


    // Empty opcodes
    uint8_t op_d3(CPU* cpu) {return 0;}
    uint8_t op_e3(CPU* cpu) {return 0;} 
    uint8_t op_e4(CPU* cpu) {return 0;}
    uint8_t op_f4(CPU* cpu) {return 0;}
    uint8_t op_db(CPU* cpu) {return 0;}
    uint8_t op_eb(CPU* cpu) {return 0;}
    uint8_t op_ec(CPU* cpu) {return 0;}
    uint8_t op_fc(CPU* cpu) {return 0;}
    uint8_t op_dd(CPU* cpu) {return 0;}
    uint8_t op_ed(CPU* cpu) {return 0;}
    uint8_t op_fd(CPU* cpu) {return 0;}

    


    