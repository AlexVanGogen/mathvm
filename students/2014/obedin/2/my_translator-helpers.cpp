
void
TVisitor::castTos(VarType to, bool stringToo)
{
    if (m_tosType == to)
        return;
    switch (to) {
        case VT_INT:
            if (m_tosType == VT_DOUBLE)
                bc()->addInsn(BC_D2I);
            else if (stringToo && m_tosType == VT_STRING)
                bc()->addInsn(BC_S2I);
            else
                throw std::runtime_error(MSG_INVALID_CAST);
            break;
        case VT_DOUBLE:
            if (m_tosType == VT_INT)
                bc()->addInsn(BC_I2D);
            else
                throw std::runtime_error(MSG_INVALID_CAST);
            break;
        default:
            throw std::runtime_error(MSG_INVALID_CAST);
    }
    m_tosType = to;
}

void
TVisitor::booleanizeTos()
{
    castTos(VT_INT, true);
    Label lSetFalse(bc()), lEnd(bc());
    bc()->addInsn(BC_ILOAD0);
    bc()->addBranch(BC_IFICMPE, lSetFalse);
    bc()->addInsn(BC_ILOAD1);
    bc()->addBranch(BC_JA, lEnd);
    bc()->bind(lSetFalse);
    bc()->addInsn(BC_ILOAD0);
    bc()->bind(lEnd);
}

void
TVisitor::genBooleanOp(TokenKind op)
{
    switch (op) {
        case tOR:
            bc()->addInsn(BC_IADD); break;
        case tAND:
            bc()->addInsn(BC_IMUL); break;
        default: break;
    }
    booleanizeTos();
}

void
TVisitor::genBitwiseOp(TokenKind op)
{
    // TODO: optimization -- don't cast and swap if they're already ints
    castTos(VT_INT);
    swapTos();
    castTos(VT_INT);
    swapTos();
    switch (op) {
        case tAOR:
            bc()->addInsn(BC_IAOR); break;
        case tAAND:
            bc()->addInsn(BC_IAAND); break;
        case tAXOR:
            bc()->addInsn(BC_IAXOR); break;
        default: break;
    }
}

void
TVisitor::genComparisonOp(TokenKind op, VarType lhsType, VarType rhsType)
{
    castTosAndPrevToSameNumType(rhsType, lhsType);
    Label lSetTrue(bc()), lEnd(bc());
    bc()->addInsn(NUMERIC_INSN(m_tosType, CMP));
    bc()->addInsn(BC_ILOAD0);
    switch (op) {
        case tEQ:
            bc()->addBranch(BC_IFICMPE, lSetTrue); break;
        case tNEQ:
            bc()->addBranch(BC_IFICMPNE, lSetTrue); break;
        case tGT:
            bc()->addBranch(BC_IFICMPL, lSetTrue); break;
        case tGE:
            bc()->addBranch(BC_IFICMPLE, lSetTrue); break;
        case tLT:
            bc()->addBranch(BC_IFICMPG, lSetTrue); break;
        case tLE:
            bc()->addBranch(BC_IFICMPGE, lSetTrue); break;
        default: break;
    }
    bc()->addInsn(BC_ILOAD0);
    bc()->addBranch(BC_JA, lEnd);
    bc()->bind(lSetTrue);
    bc()->addInsn(BC_ILOAD1);
    bc()->bind(lEnd);
    m_tosType = VT_INT;
}

void
TVisitor::genNumericOp(TokenKind op, VarType lhsType, VarType rhsType)
{
    castTosAndPrevToSameNumType(rhsType, lhsType);
    switch (op) {
        case tADD:
            bc()->addInsn(NUMERIC_INSN(m_tosType, ADD)); break;
        case tSUB:
            bc()->addInsn(NUMERIC_INSN(m_tosType, SUB)); break;
        case tMUL:
            bc()->addInsn(NUMERIC_INSN(m_tosType, MUL)); break;
        case tDIV:
            bc()->addInsn(NUMERIC_INSN(m_tosType, DIV)); break;
        default: break;
    }
}

void
TVisitor::castTosAndPrevToSameNumType(VarType prev, VarType tos)
{
    if (!IS_NUMERIC(tos) || !IS_NUMERIC(prev))
        throw std::runtime_error(MSG_NAN_ON_TOS_OR_PREV);

    if (tos == prev) {
        m_tosType = tos;
    } else if (prev == VT_DOUBLE) {
        castTos(VT_DOUBLE);
    } else {
        swapTos();
        m_tosType = prev;
        castTos(VT_DOUBLE);
        swapTos();
        m_tosType = tos;
    }
}

void
TVisitor::loadVar(const AstVar *astVar)
{
    TVar var = m_curScope->findVar(astVar);
    VarType type = astVar->type();
    if (var.contextId == m_curScope->id()) {
        switch(var.id) {
            case 0: bc()->addInsn(LOAD_VAR(type, 0)); break;
            case 1: bc()->addInsn(LOAD_VAR(type, 1)); break;
            case 2: bc()->addInsn(LOAD_VAR(type, 2)); break;
            case 3: bc()->addInsn(LOAD_VAR(type, 3)); break;
            default:
                bc()->addInsn(LOAD_VAR(type, ));
                bc()->addUInt16(var.id);
                break;
        }
    } else {
        bc()->addInsn(LOAD_CTX_VAR(type));
        bc()->addUInt16(var.contextId);
        bc()->addUInt16(var.id);
    }
    m_tosType = type;
}

void
TVisitor::storeVar(const AstVar *astVar, bool doCastTos)
{
    TVar var = m_curScope->findVar(astVar);
    VarType type = astVar->type();
    if (doCastTos)
        castTos(type); // TODO: string too?

    if (var.contextId == m_curScope->id()) {
        switch(var.id) {
            case 0: bc()->addInsn(STORE_VAR(type, 0)); break;
            case 1: bc()->addInsn(STORE_VAR(type, 1)); break;
            case 2: bc()->addInsn(STORE_VAR(type, 2)); break;
            case 3: bc()->addInsn(STORE_VAR(type, 3)); break;
            default:
                bc()->addInsn(STORE_VAR(type, ));
                bc()->addUInt16(var.id);
                break;
        }
    } else {
        bc()->addInsn(STORE_CTX_VAR(type));
        bc()->addUInt16(var.contextId);
        bc()->addUInt16(var.id);
    }
}

void
TVisitor::initVars(Scope *scope)
{
    Scope::VarIterator it(scope);
    while (it.hasNext())
        m_curScope->addVar(it.next());
}

void
TVisitor::initFunctions(Scope *scope)
{
    Scope::FunctionIterator it(scope);
    while (it.hasNext()) {
        AstFunction *fn = it.next();
        BytecodeFunction *bcFn = (BytecodeFunction*) m_code->functionByName(fn->name());
        if (bcFn == NULL) {
            bcFn = new BytecodeFunction(fn);
            m_code->addFunction(bcFn);
        }
    }

    it = Scope::FunctionIterator(scope);
    while (it.hasNext())
        it.next()->node()->visit(this);
}

void
TVisitor::genBlock(BlockNode *node)
{
    for (size_t i = 0; i < node->nodes(); ++i)
        node->nodeAt(i)->visit(this);
}

void
TScope::addVar(const AstVar *var)
{
    vars.insert(std::make_pair(var->name(), vars.size()));
}

TVar
TScope::findVar(const AstVar *var)
{
    std::map<std::string, Id>::iterator match =
        vars.find(var->name());
    if (match != vars.end())
        return TVar(match->second, id());
    else if (parent != NULL)
        return parent->findVar(var);
    else
        throw std::runtime_error(MSG_VAR_NOT_FOUND);
}
