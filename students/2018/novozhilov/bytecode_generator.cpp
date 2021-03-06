//
// Created by jetbrains on 18.10.18.
//

#include <iostream>
#include <mathvm.h>
#include "parser.h"
#include "include/bytecode_generator.h"
#include "include/bytecode_interpreter.h"

using namespace mathvm;
auto &out = std::cout;

Status* BytecodeTranslatorImpl::translate(const std::string &program, Code **code) {
    Parser parser;

    Status *status = parser.parseProgram(program);
    if (status->isOk()) {
        *code = new InterpreterCodeImpl;
        BytecodeGenerator bytecodeGenerator(*code);
        bytecodeGenerator.generateCodeForTopFunction(parser.top());
    }
    return status;
}

BytecodeGenerator::BytecodeGenerator(Code *code) : _code(code), _context(nullptr), _info(), _infoCollector(new TypeInfoCollector(this)) {}

void BytecodeGenerator::generateCodeForTopFunction(AstFunction *function) {
    auto* bytecodeFunction = new BytecodeFunction(function);
    _code->addFunction(bytecodeFunction);

    auto *functionCollector = new FunctionCollector(this);
    function->node()->visit(functionCollector);
    delete functionCollector;

    auto *callCollector = new FunctionCallCollector(this);
    function->node()->visit(callCollector);
    delete callCollector;

    generateCodeForFunction(function);
    bytecodeFunction->bytecode()->addInsn(BC_STOP);
}

void BytecodeGenerator::generateCodeForFunction(AstFunction *function) {
    auto *bytecodeFunction = dynamic_cast<BytecodeFunction*>(_code->functionByName(function->name()));

    _context = new Context(bytecodeFunction, _context, function->scope());

    for (uint i = 0; i < function->parametersNumber(); ++i) {
        AstVar* var = function->scope()->lookupVariable(function->parameterName(i), false);
        storeValueToVar(var);
    }

    function->node()->visit(_infoCollector);
    function->node()->visit(this);

    bytecodeFunction->setScopeId(_context->getContextId());
    bytecodeFunction->setLocalsNumber(_context->getVarsCount());

    Context* parentContext = _context->getParentContext();
    delete _context;
    _context = parentContext;
}

void BytecodeGenerator::visitForNode(ForNode *node) {
    // TODO: add check for type

    BinaryOpNode *inExpr = node->inExpr()->asBinaryOpNode();
    inExpr->left()->visit(this);

    const AstVar *var = node->var();
    VarType type = var->type();
    storeValueToVar(var);

    Bytecode *bytecode = getBytecode();

    Label startLabel(bytecode);
    Label endLabel(bytecode);

    bytecode->bind(startLabel);
    loadValueFromVar(var, type);
    inExpr->right()->visit(this);

    bytecode->addBranch(BC_IFICMPL, endLabel);
    node->body()->visit(this);

    loadValueFromVar(var, type);
    bytecode->addInsn(BC_ILOAD1);
    bytecode->addInsn(BC_IADD);
    storeValueToVar(var);
    bytecode->addBranch(BC_JA, startLabel);
    bytecode->bind(endLabel);
}

void BytecodeGenerator::visitWhileNode(WhileNode *node) {
    Bytecode *bytecode = getBytecode();

    Label startLabel = Label(bytecode);
    Label endLabel = Label(bytecode);

    bytecode->bind(startLabel);
    node->whileExpr()->visit(this);
    bytecode->addInsn(BC_ILOAD0);
    bytecode->addBranch(BC_IFICMPE, endLabel);

    node->loopBlock()->visit(this);
    bytecode->addBranch(BC_JA, startLabel);
    bytecode->bind(endLabel);
}

void BytecodeGenerator::visitIfNode(IfNode *node) {
    node->ifExpr()->visit(this);
    // TODO: add check for type

    Bytecode *bytecode = getBytecode();
    Label elseLabel(bytecode);
    Label endLabel(bytecode);

    bytecode->addInsn(BC_ILOAD0);
    bytecode->addBranch(BC_IFICMPE, elseLabel);

    node->thenBlock()->visit(this);
    bytecode->addBranch(BC_JA, endLabel);
    bytecode->bind(elseLabel);
    if (node->elseBlock()) {
        node->elseBlock()->visit(this);
    }
    bytecode->bind(endLabel);
}

void BytecodeGenerator::visitPrintNode(PrintNode *node) {
    Bytecode *bytecode = getBytecode();
    
    uint32_t operands = node->operands();
    for (uint32_t i = 0; i < operands; ++i) {
        AstNode *operand = node->operandAt(i);
        operand->visit(this);
        switch (getNodeType(operand)) {
            case VT_INT:
                bytecode->addInsn(BC_IPRINT);
                break;
            case VT_DOUBLE:
                bytecode->addInsn(BC_DPRINT);
                break;
            case VT_STRING:
                bytecode->addInsn(BC_SPRINT);
                break;
            default:
                throw std::runtime_error("undefined variable type");
        }
    }
}

void BytecodeGenerator::visitLoadNode(LoadNode *node) {
    loadValueFromVar(node->var(), getNodeType(node));
}

void BytecodeGenerator::visitStoreNode(StoreNode *node) {
    node->value()->visit(this);
    TokenKind op = node->op();
    Bytecode *bytecode = getBytecode();
    const AstVar *var = node->var();
    switch (op) {
        case tASSIGN:
            break;
        case tINCRSET:
        case tDECRSET: {
            VarType type = var->type();
            loadValueFromVar(var, type);
            switch (type) {
                case VT_INT:
                    bytecode->addInsn(op == tINCRSET ? BC_IADD : BC_ISUB);
                    break;
                case VT_DOUBLE:
                    bytecode->addInsn(op == tINCRSET ? BC_DADD : BC_DSUB);
                    break;
                default:
                    bytecode->addInsn(BC_INVALID);
            }
            break;
        }
        default:
            throw std::runtime_error("undefined variable type");
    }
    storeValueToVar(var);
}

void BytecodeGenerator::visitIntLiteralNode(IntLiteralNode *node) {
    Bytecode *bytecode = getBytecode();
    bytecode->addInsn(BC_ILOAD);
    bytecode->addInt64(node->literal());
    castVarOnStackTop(VT_INT, getNodeType(node));
}

void BytecodeGenerator::visitDoubleLiteralNode(DoubleLiteralNode *node) {
    Bytecode *bytecode = getBytecode();
    bytecode->addInsn(BC_DLOAD);
    bytecode->addDouble(node->literal());
    castVarOnStackTop(VT_DOUBLE, getNodeType(node));
}

void BytecodeGenerator::visitStringLiteralNode(StringLiteralNode *node) {
    Bytecode *bytecode = getBytecode();
    bytecode->addInsn(BC_SLOAD);
    bytecode->addUInt16(_code->makeStringConstant(node->literal()));
    castVarOnStackTop(VT_STRING, getNodeType(node));
}

void BytecodeGenerator::visitUnaryOpNode(UnaryOpNode *node) {
    node->operand()->visit(this);
    Bytecode *bytecode = getBytecode();
    VarType type = getNodeType(node);

    switch (node->kind()) {
        case tSUB: {
            switch (type) {
                case VT_INT:
                    bytecode->addInsn(BC_INEG);
                    break;
                case VT_DOUBLE:
                    bytecode->addInsn(BC_DNEG);
                    break;
                default:
                    throw std::runtime_error("illegal type on stack");
            }
            break;
        }

        case tNOT: {
            if (type != VT_INT) {
                throw std::runtime_error("illegal type on stack");
            }
            bytecode->addInsn(BC_ILOAD1);
            bytecode->addInsn(BC_IAXOR);
            break;
        }
        default:
            throw std::runtime_error("illegal operation kind");
    }
}

void BytecodeGenerator::visitBinaryOpNode(BinaryOpNode *node) {
    switch (node->kind()) {
        case tADD:
        case tSUB:
        case tMUL:
        case tDIV:
        case tMOD:
            processArithmeticOperation(node);
            break;
        case tOR:
        case tAND:
        case tAOR:
        case tAAND:
        case tAXOR:
            processLogicOperation(node);
            break;
        case tEQ:
        case tNEQ:
        case tGT:
        case tGE:
        case tLT:
        case tLE:
            processComparingOperation(node);
            break;
        default:
            throw std::runtime_error("illegal operation kind");
    }
}

void BytecodeGenerator::processArithmeticOperation(BinaryOpNode *node) {
    VarType type = getNodeType(node);

    node->right()->visit(this);
    castVarOnStackTop(getNodeType(node->right()), type);
    node->left()->visit(this);
    castVarOnStackTop(getNodeType(node->left()), type);

    if (!(type == VT_INT || type == VT_DOUBLE)) {
        throw std::runtime_error("illegal type on stack");
    }
    bool intType = type == VT_INT;

    Bytecode *bytecode = getBytecode();
    
    switch (node->kind()) {
        case tADD:
            bytecode->addInsn(intType ? BC_IADD : BC_DADD);
            break;
        case tSUB:
            bytecode->addInsn(intType ? BC_ISUB : BC_DSUB);
            break;
        case tMUL:
            bytecode->addInsn(intType ? BC_IMUL : BC_DMUL);
            break;
        case tDIV:
            bytecode->addInsn(intType ? BC_IDIV : BC_DDIV);
            break;
        case tMOD: {
            if (!intType) {
                throw std::runtime_error("illegal type on stack");
            }
            bytecode->addInsn(BC_IMOD);
            break;
        }
        default:
            throw std::runtime_error("illegal operation kind");
    }
}

void BytecodeGenerator::processLogicOperation(BinaryOpNode *node) {
    node->visitChildren(this);

    if (getNodeType(node) != VT_INT) {
        throw std::runtime_error("illegal type on stack");
    }

    Bytecode *bytecode = getBytecode();

    switch (node->kind()) {
        case tAOR:
        case tOR:
            bytecode->addInsn(BC_IAOR);
            break;
        case tAAND:
        case tAND:
            bytecode->addInsn(BC_IAAND);
            break;
        case tAXOR:
            bytecode->addInsn(BC_IAXOR);
            break;
        default:
            throw std::runtime_error("illegal operation kind");
    }
}

void BytecodeGenerator::processComparingOperation(BinaryOpNode *node) {
    Bytecode *bytecode = getBytecode();

    VarType leftType = getNodeType(node->left());
    VarType rightType = getNodeType(node->right());

    if (!isNumberType(leftType) || !isNumberType(rightType)) {
        throw std::runtime_error("illegal type on stack");
    }

    VarType type;
    Instruction comparingInstruction;
    if (leftType == VT_DOUBLE || rightType == VT_DOUBLE) {
        type = VT_DOUBLE;
        comparingInstruction = BC_DCMP;
    } else {
        type = VT_INT;
        comparingInstruction = BC_ICMP;
    }

    node->right()->visit(this);
    castVarOnStackTop(getNodeType(node->right()), type);
    node->left()->visit(this);
    castVarOnStackTop(getNodeType(node->left()), type);

    bytecode->addInsn(comparingInstruction);
    bytecode->addInsn(BC_ILOAD0);

    Instruction instruction;

    switch (node->kind()) {
        case tEQ:
            instruction = BC_IFICMPE;
            break;
        case tNEQ:
            instruction = BC_IFICMPNE;
            break;
        case tGT:
            instruction = BC_IFICMPG;
            break;
        case tGE:
            instruction = BC_IFICMPGE;
            break;
        case tLT:
            instruction = BC_IFICMPL;
            break;
        case tLE:
            instruction = BC_IFICMPLE;
            break;
        default:
            throw std::runtime_error("illegal operation kind");
    }
    Label elseLabel(bytecode);
    Label endLabel(bytecode);
    bytecode->addBranch(instruction, elseLabel);
    // false
    bytecode->addInsn(BC_ILOAD0);
    bytecode->addBranch(BC_JA, endLabel);
    // true
    bytecode->bind(elseLabel);
    bytecode->addInsn(BC_ILOAD1);
    bytecode->bind(endLabel);
}

void BytecodeGenerator::visitCallNode(CallNode *node) {
    TranslatedFunction *function = _code->functionByName(node->name());
    if (function == nullptr) {
        throw std::runtime_error("no function named " + node->name());
    }

    for (uint32_t i = node->parametersNumber(); i > 0; --i) {
        AstNode *parameter = node->parameterAt(i - 1);
        parameter->visit(this);
    }

    Bytecode *bytecode = getBytecode();
    bytecode->addInsn(BC_CALL);
    bytecode->addUInt16(function->id());

    castVarOnStackTop(function->returnType(), getNodeType(node));

    if (!_info.returnValueUsed[node->position()] && function->returnType() != VT_VOID) {
        bytecode->addInsn(BC_POP);
    }
}

void BytecodeGenerator::visitNativeCallNode(NativeCallNode *node __attribute__ ((unused))) {
    // TODO
}

void BytecodeGenerator::visitReturnNode(ReturnNode *node) {
    AstNode *returnExpr = node->returnExpr();
    if (returnExpr != nullptr) {
        returnExpr->visit(this);
    }
    getBytecode()->addInsn(BC_RETURN);
}

void BytecodeGenerator::visitFunctionNode(FunctionNode *node) {
    node->body()->visit(this);
}

void BytecodeGenerator::visitBlockNode(BlockNode *node) {
    Scope::VarIterator varIterator(node->scope());
    while (varIterator.hasNext()) {
        _context->addVar(varIterator.next());
    }

    Scope::FunctionIterator functionIterator(node->scope());
    while (functionIterator.hasNext()) {
        AstFunction *function = functionIterator.next();
        generateCodeForFunction(function);
    }

    node->visitChildren(this);
}

// ----------------------------- private -----------------------------

Bytecode *BytecodeGenerator::getBytecode() {
    return _context->getBytecode();
}

void BytecodeGenerator::storeValueToVar(AstVar const *var) {
    Context* context = findOwnerContextOfVar(var->name());
    assert(context != nullptr);
    bool inSameContext = context == _context;

    Instruction insn;
    switch (var->type()) {
        case VT_INT:
            insn = inSameContext ? BC_STOREIVAR : BC_STORECTXIVAR;
            break;
        case VT_DOUBLE:
            insn = inSameContext ? BC_STOREDVAR : BC_STORECTXDVAR;
            break;
        case VT_STRING:
            insn = inSameContext ? BC_STORESVAR : BC_STORECTXSVAR;
            break;
        default:
            throw std::runtime_error("undefined variable type");
    }

    getBytecode()->addInsn(insn);
    getBytecode()->addUInt16(context->getVarId(var->name()));
    if (!inSameContext) {
        getBytecode()->addUInt16(context->getContextId());
    }
}

void BytecodeGenerator::loadValueFromVar(AstVar const *var, VarType targetType) {
    Context* context = findOwnerContextOfVar(var->name());
    assert(context != nullptr);
    bool inSameContext = context == _context;

    Instruction insn;
    switch (var->type()) {
        case VT_INT:
            insn = inSameContext ? BC_LOADIVAR : BC_LOADCTXIVAR;
            break;
        case VT_DOUBLE:
            insn = inSameContext ? BC_LOADDVAR : BC_LOADCTXDVAR;
            break;
        case VT_STRING:
            insn = inSameContext ? BC_LOADSVAR : BC_LOADCTXSVAR;
            break;
        default:
            throw std::runtime_error("undefined variable type");
    }

    getBytecode()->addInsn(insn);
    getBytecode()->addUInt16(context->getVarId(var->name()));
    if (!inSameContext) {
        getBytecode()->addUInt16(context->getContextId());
    }
    castVarOnStackTop(var->type(), targetType);
}

VarType BytecodeGenerator::getNodeType(AstNode *node) {
    return _info.expressionType[node->position()];
}

Context *BytecodeGenerator::findOwnerContextOfVar(string name) {
    for (Context* context = _context; context != nullptr; context = context->getParentContext()) {
        if (context->containsVariable(name)) {
            return context;
        }
    }
    return nullptr;
}

void BytecodeGenerator::castVarOnStackTop(VarType sourceType, VarType targetType) {
    if (sourceType == targetType) {
        return;
    }

    Bytecode *bytecode = getBytecode();
    if (sourceType == VT_INT && targetType == VT_DOUBLE) {
        bytecode->addInsn(BC_I2D);
    } else if (sourceType == VT_DOUBLE && targetType == VT_INT) {
        bytecode->addInsn(BC_D2I);
    } else if (sourceType == VT_STRING && targetType == VT_INT) {
        bytecode->addInsn(BC_S2I);
    } else if (sourceType == VT_STRING && targetType == VT_DOUBLE) {
        bytecode->addInsn(BC_S2I);
        bytecode->addInsn(BC_I2D);
    } else {
        throw std::runtime_error("cast error");
    }
}

bool BytecodeGenerator::isNumberType(VarType type) {
    return type == VT_INT || type == VT_DOUBLE;
}
