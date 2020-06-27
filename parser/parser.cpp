#include "parser.h"

Instruction::~Instruction(){}

//tested
bool Instruction::isMemAddr(const QString &param) {
    if(param.startsWith('[') && param.endsWith(']'))
        return true;
    return false;
}

std::tuple<QString, QString> Instruction::twoTokens() {
    QStringList list = codeLine.split(QRegExp(" "), QString::SkipEmptyParts);
    return {list.at(0), list.at(1)};
}

//tested
std::tuple<QString, QString, QString> Instruction::threeTokens() {
    QStringList list = codeLine.split(QRegExp(" |\\,"), QString::SkipEmptyParts);
    return {list[0].toUpper(), list[1].toUpper(), list[2].toUpper()};
}

//tested
enum OperandType Instruction::getOperandType(const QString& operand) {
    if(Regs8.contains(operand.trimmed().toUpper())) return OperandType::Reg8;
    else if(Regs16.contains(operand.trimmed().toUpper())) return OperandType::Reg16;
    else if(SegRegs.contains(operand.trimmed().toUpper())) return OperandType::segReg;
    else if(isMemAddr(operand)) return OperandType::MemAddr;
    else if(isImmed8(operand)) return OperandType::Immed8;
    else if(isImmed16(operand)) return OperandType::Immed16;
    return OperandType::Unknown;
}

uchar Instruction::rmGenerator(const QString& param) {
    QString addr = param.trimmed().toUpper();

    QStringList argList = addr.split("+", QString::SkipEmptyParts);
    if(argList.size() >= 2) {
        if(argList.contains("BX") && argList.contains("SI")) return mod00["BX+SI"];
        else if(argList.contains("BX") && argList.contains("DI")) return mod00["BX+DI"];
        else if(argList.contains("BP") && argList.contains("SI")) return mod00["BP+SI"];
        else if(argList.contains("BP") && argList.contains("DI")) return mod00["BP+DI"];

        else if(argList.contains("BX") && !argList.contains("SI") && !argList.contains("DI")) return mod00["BX"];
        else if(argList.contains("DI") && !argList.contains("BX") && !argList.contains("BP")) return mod00["DI"];
        else if(argList.contains("SI") && !argList.contains("BX") && !argList.contains("BP")) return mod00["SI"];
    }
    else if(argList.size() == 1) {
        if(argList.contains("BX")) return mod00["BX"];
        else if(argList.contains("SI")) return mod00["SI"];
        else if(argList.contains("DI")) return mod00["DI"];
        else return mod00["DA"];
    }
    //on error
    return 0xff;
}

bool Instruction::isImmed8(const QString& param) {
    return isHexValue(param) && (param.length() <= 2);
}

inline bool Instruction::isImmed16(const QString& param) {
    return isHexValue(param) && (param.length() > 2 && param.length() <= 4);
}

void Instruction::setCodeLine(const QString& param) {
    codeLine = param;
}

const QString& Instruction::getCodeLine() { return codeLine; }

uchar Instruction::modRegRmGenerator(const uchar& mod, const uchar& reg, const uchar& rm) {
    return ((mod << 6) | (reg << 3) | (rm));
}

QString Mov::process() {
    uchar modregrm;
    uchar mod = 0x00;
    QString machineCode;
    uchar opcode;

    //split the instruction into
    auto [mnemonic, firstOp, secondOp] = threeTokens();
    //get the operand type. is it a reg16, reg8, mem address, segreg or what?
    OperandType firstOpType = getOperandType(firstOp); OperandType secondOpType = getOperandType(secondOp);
    //in the form of [firstOpType]-[secondOpType]
    QString generalExpression = Operands[firstOpType] + '-' + Operands[secondOpType];

    //if both the operands are of the same size
    if(((firstOpType==OperandType::Reg16) && (secondOpType==OperandType::Reg16)) ||
            ((firstOpType==OperandType::Reg8) && (secondOpType==OperandType::Reg8))) {
        //the mod is indeed 3, and the machine code is 2 bytes
        //the opcode, and the modregrm byte
        opcode = getOpcode(generalExpression);
        machineCode.append(hexToStr(opcode));
        machineCode.append(hexToStr(modRegRmGenerator(0x3, getGpRegCode(secondOp), getGpRegCode(firstOp))));
        return machineCode;
    }

    else if(firstOpType==OperandType::Reg8 && secondOpType==OperandType::Immed8) {
        opcode = getOpcode(firstOp+"-IMMED8");
        machineCode.append(hexToStr(opcode));
        machineCode.append(secondOp);
        return machineCode;
    }

    else if(firstOpType==OperandType::Reg16 && (secondOp==OperandType::Immed16 || secondOpType==Immed8)){
        opcode = getOpcode(firstOp.toUpper()+"-IMMED16");
        machineCode.append(hexToStr(opcode)); //the instruction machine code, in the case its mov XX, XX is any 8 bit reg
        machineCode.append(hexToStr(secondOp.toInt(nullptr, 16), HexType::DirectAddress));

        return machineCode;
    }

    else if(firstOpType==OperandType::MemAddr || secondOpType==OperandType::MemAddr) {
        bool destIsMemAddr = firstOpType==OperandType::MemAddr;
        QStringList addrArgs;
        uchar reg;

        if(destIsMemAddr) {
            firstOp.remove("["); firstOp.remove("]");
            addrArgs = firstOp.split("+");
        } else {
            secondOp.remove("["); secondOp.remove("]");
            addrArgs = secondOp.split("+");
        }

        QStringList displacment = addrArgs.filter(QRegularExpression("[0-9a-fA-F]"));
        hexValidator(displacment);

        bool directAddress = addrArgs.size() == 1;

        int displacmentValue = 0;
        if(displacment.size() >= 1)
            displacmentValue = displacment.first().toInt(0, 16);

        auto Mod1 = [displacmentValue]()-> bool {
            return ((displacmentValue >= 0x0000 && displacmentValue <= 0x007F) ||
                    (displacmentValue >= 0xFF80 && displacmentValue <= 0xFFFF));
        };

        if(displacment.empty()) mod = 0x00;         //for bx+si, bp+di ... and si, di as well
        else if(!displacment.empty()) {
            if(displacment.size() == 1 && addrArgs.size() == 1) mod = 0x00; //direct address;
            else mod = (Mod1() ? 0x01 : 0x02);       //the rest of the cases!
        }

        if (firstOp == "AL" && isHexValue(secondOp)) { machineCode.append(hexToStr(getOpcode(firstOp+"-MEM")));
            machineCode.append(hexToStr(extractDisplacment(secondOp).toInt(0, 16), HexType::DirectAddress)); return machineCode;}
        if (firstOp == "AX" && isHexValue(secondOp)) { machineCode.append(hexToStr(getOpcode(firstOp+"-MEM")));
            machineCode.append(hexToStr(extractDisplacment(secondOp).toInt(0, 16), HexType::DirectAddress)); return machineCode;}

        if(secondOp == "AL" && isHexValue(firstOp)) { machineCode.append(hexToStr(getOpcode("MEM-"+secondOp)));
            machineCode.append(hexToStr(extractDisplacment(firstOp).toInt(0, 16), HexType::DirectAddress)); return machineCode;}
        if(secondOp == "AX" && isHexValue(firstOp)) { machineCode.append(hexToStr(getOpcode("MEM-"+secondOp)));
            machineCode.append(hexToStr(extractDisplacment(firstOp).toInt(0, 16), HexType::DirectAddress)); return machineCode;}
        //---

        if(destIsMemAddr) {
            if(secondOpType == OperandType::segReg)
                reg = getSegRegCode(secondOp);
            else
                reg = getGpRegCode(secondOp);
            modregrm = modRegRmGenerator(mod, reg, rmGenerator(firstOp));
        } else {
            if(firstOpType == OperandType::segReg)
                reg = getSegRegCode(firstOp);
            else
                reg = getGpRegCode(firstOp);
            modregrm = modRegRmGenerator(mod, reg, rmGenerator(secondOp));
        }

        opcode = getOpcode(generalExpression);
        machineCode.append(hexToStr(opcode));
        machineCode.append(hexToStr(modregrm));
        if(!displacment.empty())
            machineCode.append(hexToStr(displacment.first().toInt(0, 16), (directAddress ? HexType::DirectAddress : Address)));
        return machineCode;
    }

    else if(firstOpType==OperandType::Reg16 && secondOpType==OperandType::segReg) {
        opcode = getOpcode(generalExpression);
        modregrm = modRegRmGenerator(0x03, getSegRegCode(secondOp), getGpRegCode(firstOp));
        machineCode.append(hexToStr(opcode)); machineCode.append(hexToStr(modregrm));
        return machineCode;
    }

    else if(firstOpType==OperandType::segReg && secondOpType==OperandType::Reg16) {
        opcode = getOpcode(generalExpression);
        modregrm = modRegRmGenerator(0x03, getSegRegCode(firstOp), getGpRegCode(secondOp));
        machineCode.append(hexToStr(opcode)); machineCode.append(hexToStr(modregrm));
        return machineCode;
    }

    return machineCode;
}

uchar Mov::getOpcode(const QString& param, bool *ok) {
    auto match = LUT.find(param.toUpper().toStdString());
    if(match != std::end(LUT)) {
        if(ok != nullptr) *ok = true;
        return match->second;
    }
    if(ok != nullptr) *ok = false;
    return 0x0;
}

uchar Instruction::getGpRegCode(const QString& param, bool *ok) {
    auto match = gpRegsHex.find(param.toUpper().toStdString());
    if(match != std::end(gpRegsHex)) {
        if(ok != nullptr) *ok = false;
        return match->second;
    }
    if(ok != nullptr) *ok = false;
    return 0x00;
}

QString Instruction::extractDisplacment(const QString& param, bool *ok) {
    QStringList splittedAddress = param.split('+');
    QStringList filteredDisplacment = splittedAddress.filter(QRegularExpression("[0-9a-fA-F]"));

    if(filteredDisplacment.size() > 0) {
        filteredDisplacment.first().remove(']'); filteredDisplacment.first().remove('[');
        if(ok != nullptr) *ok=true;
        return filteredDisplacment.first();
    }
    if(ok != nullptr) *ok = false;
    return "";
}

uchar Instruction::getSegRegCode(const QString& param, bool *ok) {
    auto match = segRegsHex.find(param.toUpper().toStdString());
    if(match != std::end(segRegsHex)) {
        if(ok != nullptr) *ok = false;
        return match->second;
    }
    if(ok != nullptr) *ok = false;
    return 0x00;
}

bool Instruction::isHexValue(const QString& param) {
    bool b;
    param.toInt(&b, 16);
    return b;
}

void Instruction::hexValidator(QStringList& param) {
    param.erase(
            std::remove_if(
                std::begin(param),
                std::end(param),
                [this](QString const &p) {return !this->isHexValue(p);}), std::end(param));
}

Mov::~Mov(){}
Mov::Mov(){}
Instruction::Instruction(){}
