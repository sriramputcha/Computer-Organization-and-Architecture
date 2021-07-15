/* This code "simulates" the working of a Pipelined processor
 * It can be used to find the number of clock cycles, RAWStalls, instruction count.
 * It modifies the data cache and RF accordingly
 * Team: Vineeth, Ravi, Gautam */

#include <bits/stdc++.h>
#include <sys/stat.h>
#include <sys/types.h>
using namespace std;

/* For conversions between hexadecimal character and decimal number */
#define to_dec(ch) ((ch) < 'a' ? (ch)- '0' : (ch) - 'a' + 10)
#define to_hex(d) ((d) < 10 ? (char)((d)+'0') : (char)((d) - 10 + 'a'))

int main(){
    
    // FILE IO
    mkdir("output", 0777);
    ifstream ICacheFile("input/ICache.txt");
    fstream DCacheFile("input/DCache.txt");
    ifstream RegisterFile("input/RF.txt");
    ofstream OutputFile("output/Output.txt");
    ofstream OutputDCacheFile("output/DCache.txt");

    // Variables used for storing input data
    int ic[16] = {};    // Insturction count of all 16 Insturctions
    string iCache[256];    // Instruction cache -- Data from file mapped to array
    string dCache[256];    // Data cache -- Data from file mapped to array
    int RF[16]; // Reg.File -- Data from file mapped to array
    
    // Taking data from files
    for(int i = 0; i < 16; i++){
        string t; RegisterFile >> t;
        RF[i] = to_dec(t[0])*16 + to_dec(t[1]);
        RF[i] -= 256*(RF[i] >> 7);
    }
    for(int i = 0; i < 256; i++){
        ICacheFile >> iCache[i];
        DCacheFile >> dCache[i];
    }

    // Variables used other than IO
    string ir;  // Instruction register
    int pc = 0; // Programme counter
    int isOpen[16] = {}; // Checks whether or not data will be written to Register in future cycles
    int dest = 0, destcopy = 0, destcopycopy = 0;   // Destination register index
    int IT = 0; // Type of instruction [0,15]
    int add = 0, sub = 1, mul = 2, inc = 3, And = 4, Or = 5, Not = 6, Xor = 7, load = 8, store = 9, jmp = 10, beqz = 11, hlt = 15;
    int A, B, ALUoutput, ALUoutputcopy, LMD, L1, R1, R1copy;    // Buffer registers
    int clock = 0, RAWStalls = 0, tempRAWStalls = 0, tempCtrlStalls = 0, RAW = 0, controlStalls = 0;
    int LS = 0; // 0 - Not load store, 1 - Load, 2 - store
    int RegW, RegWcopy; // Indicate write mode of register: 0 - ALUoutput to Reg, 1 - LMD to Reg
    int dependsOn = 0;  // Used in data dependencies
    bool ID, EX, MEM, WB;   // = true means particular stage can be executed in current cycle

    while(true) {    // Repeat until we encounter a halt, Each loop is a clock cycle
        clock++;
        
        if(RAW == 1) tempRAWStalls++;
        tempCtrlStalls = max(tempCtrlStalls - 1, 0);

        // WB stage of I - 4
        if(WB == 1){
            if(RegWcopy == 1)// Write LMD of instruction I-4 to register
                RF[destcopycopy] = LMD;
            else if(RegWcopy == 0) // Write ALUOutput of instruction I-4 to register
                RF[destcopycopy] = ALUoutputcopy;
            if(RegWcopy == 1 || RegWcopy == 0){
                isOpen[destcopycopy] = false;
                if(RAW == 1 && ((1 << destcopycopy) & dependsOn)){
                    dependsOn -= (1 << destcopycopy);
                    if(dependsOn == 0){ // If the dependent registers are written then RAW is resolved
                        RAWStalls += tempRAWStalls; // tempRAWStalls stores the number of cycles from getting RAW to resolving it
                        RAW = 0;    // RAW hazard is resolved
                        tempRAWStalls = 0;
                        ID = 1;  // If data dependency is resolved we can execute ID in this clock cycle
                    }
                }
            }
            WB = 0;
        }
        // MEM stage of I - 3
        if(MEM == 1){
            if(LS == 1) {   // Load Instruction
                LMD = to_dec(dCache[ALUoutput][0])*16 + to_dec(dCache[ALUoutput][1]);
                LMD -= 256*(LMD>>7);
            }
            if(LS == 2){   // Store Instruction
                dCache[ALUoutput][0] = to_hex(R1copy/16);
                dCache[ALUoutput][1] = to_hex(R1copy%16);
            }
            ALUoutputcopy = ALUoutput; RegWcopy = RegW; destcopycopy = destcopy;
            MEM = 0;
            WB = 1;
        }
        // EX of I - 2
        if(EX == 1){
            LS = 0;
            R1copy = R1;
            destcopy = dest;

            //Performing the appropriate ALU operation based on instruction-type
            if(IT == add) ALUoutput = A + B;
            if(IT == sub) ALUoutput = A - B;
            if(IT == mul) ALUoutput = A * B;
            if(IT == inc) ALUoutput = A + 1;
            if(IT == And) ALUoutput = A & B;
            if(IT == Or) ALUoutput = A | B;
            if(IT == Not) ALUoutput = !A;
            if(IT == Xor) ALUoutput = A ^ B;
            if(IT == load) { ALUoutput = A + B; LS = 1; }  // Load
            if(IT == store) { ALUoutput = A + B; LS = 2; }  // Store
            if(IT == jmp) pc = pc + (L1 << 1);   // JMP
            if(IT == beqz) if(A == 0) pc = pc + (L1 << 1); // Conditional JMP
            if(IT == hlt) break; // HALT

            if(LS == 1) RegW = 1;   // Load so we have to write from LMD to register
            else if(IT == jmp || IT == beqz || IT == store) RegW = 2;  // Incase of store, JMP, cond. JMP no need of register writing
            else RegW = 0;  // Write from ALUOutput to Register

            EX = 0;
            MEM = 1;
        }
        // Instruction Decode(ID) of I - 1
        if(ID == 1){
            IT = to_dec(ir[0]);
            // Instructions involving 3 registers
            if(IT == add || IT == sub || IT == mul || IT == And || IT == Or || IT == Xor){
                int op1 = to_dec(ir[2]); A = RF[op1];
                int op2 = to_dec(ir[3]); B = RF[op2];
                if(isOpen[op1]) {RAW = 1; dependsOn += (1 << op1);}
                if(isOpen[op2]) {RAW = 1; dependsOn += (1 << op2);}
            }
            // Instructions having first register as one of the sources
            if(IT == inc || IT == beqz){
                int op1 = to_dec(ir[1]); A = RF[op1];
                if(isOpen[op1]) {RAW = 1; dependsOn += (1 << op1);}
            }
            // NOT instruction
            if(IT == Not){
                int op1 = to_dec(ir[2]); A = RF[op1];
                if(isOpen[op1]) {RAW = 1; dependsOn += (1 << op1);}
            }
            // Load and store
            if(IT == load || IT == store){
                int op1 = to_dec(ir[2]); A = RF[op1];
                B = to_dec(ir[3]); B -= 16*(B>>3);
                if(isOpen[op1]) {RAW = 1; dependsOn += (1 << op1);}
                if(IT == store){
                    int r1_idx = to_dec(ir[1]); // Index of R1
                    if(isOpen[r1_idx]) {RAW = 1; dependsOn += (1 << r1_idx);}
                    else { R1 = RF[r1_idx]; R1 = (R1 + 256)%256; }
                }
            }

            // Finding L1 for branch instructions
            if(IT == beqz) L1 = to_dec(ir[2])*16 + to_dec(ir[3]);
            else L1 = to_dec(ir[1])*16 + to_dec(ir[2]);
            L1 -= 256*(L1 >> 7);

            // For any control statement we are taking two stalls
            if((IT == jmp || IT == beqz) && !RAW) {tempCtrlStalls = 2; controlStalls += 2;}

            dest = to_dec(ir[1]);   // Destination register
            if(IT != store && IT != jmp && IT != beqz) isOpen[dest] = true; // Destination register is open
            ID = 0;
            if(RAW) EX = 0;  // We cannot perform EX stage until RAW is resolved
            else {EX = 1;  ic[IT]++;}
        }
        // Instruction Fetch (IF) of I
        if(RAW == 0 && tempCtrlStalls == 0){
            ir = iCache[pc] + iCache[pc+1]; // inst. len = 16bits = 2bytes, so it spans across 2 lines
            pc = pc + 2;
            ID = 1;
        }
    }

    int totalIC=0;
    for(int i=0; i<16; i++) totalIC+=ic[i];
    int arithmeticIC=ic[0]+ic[1]+ic[2]+ic[3];
    int logicalIC=ic[4]+ic[5]+ic[6]+ic[7];
    int dataIC=ic[8]+ic[9];
    int controlIC=ic[10]+ic[11];
    int haltIC=ic[15];

    OutputFile<<"Total number of instructions executed: "<<totalIC<<endl;
    OutputFile<<"Number of instructions in each class"<<endl;
    OutputFile<<"Arithmetic instructions              : "<<arithmeticIC<<endl;
    OutputFile<<"Logical instructions                 : "<<logicalIC<<endl;
    OutputFile<<"Data instructions                    : "<<dataIC<<endl;
    OutputFile<<"Control instructions                 : "<<controlIC<<endl;
    OutputFile<<"Halt instructions                    : "<<haltIC<<endl;
    OutputFile<<"Cycles Per Instruction               : "<<(1.0*(clock + 2)/totalIC)<<endl;
    OutputFile<<"Total number of stalls               : "<<(RAWStalls+controlStalls)<<endl;
    OutputFile<<"Data stalls (RAW)                    : "<<RAWStalls<<endl;
    OutputFile<<"Control stalls                       : "<<controlStalls<<endl;

    for(int i=0; i<256; i++)
        OutputDCacheFile<<dCache[i]<<endl;
}
