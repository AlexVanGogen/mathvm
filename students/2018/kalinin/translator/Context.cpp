//
// Created by Владислав Калинин on 20/11/2018.
//

#include "Context.h"

using namespace mathvm;

vector<BytecodeFunction *> Context::functionList{};

unordered_map<string, uint16_t> Context::constantsById{};

vector<string> Context::constantsList{};

BytecodeFunction *Context::getFunction(string name) {
    Context *ctx = this;
    while (ctx->functionsById.find(name) == ctx->functionsById.end()) {
        if (ctx->parent == nullptr) {
            return nullptr;
        }
        ctx = ctx->parent;
    }
    return functionList[ctx->functionsById[name]];
}

uint16_t Context::getId() {
    return id;
}

uint16_t Context::makeStringConstant(string literal) {
    if (constantsById.find(literal) != constantsById.end()) {
        return constantsById[literal];
    }
    uint16_t id = static_cast<unsigned short>(constantsById.size());
    constantsById[literal] = id;
    constantsList.push_back(literal);
    return id;
}

Context *Context::getParent() {
    return parent;
}

Context *Context::getVarContext(string name) {
    auto *result = root->getVarContext(name);
    if (result != nullptr) {
        return result;
    }
    if (parent == nullptr) {
        return nullptr;
    }
    return parent->getVarContext(name);
}

uint16_t Context::varNumber() {
    return static_cast<uint16_t>(varList.size());
}

SubContext *Context::subContext() {
    if (currentSubContext == nullptr) {
        return root;
    }
    return currentSubContext;
}

void Context::createAndMoveToLowerLevel() {
    if (root == nullptr) {
        root = new SubContext(this);
        currentSubContext = root;
    } else {
        currentSubContext = currentSubContext->addChild();
    }
}

void Context::moveToUperLevel() {
    currentSubContext = currentSubContext->getParent();
}

void Context::nextSubContext() {
    if (currentSubContext == nullptr) {
        currentSubContext = root;
    } else if (currentSubContext->childsIterator()->hasNext()) {
        currentSubContext = currentSubContext->childsIterator()->next();
    } else {
        moveToUperLevel();
    }
}

Context *Context::addChild() {
    auto *child = new Context(this);
    childs.push_back(child);
    return child;
}

uint16_t Context::getVarId(string name) {
    return root->getVarId(name);
}

Context *Context::getChildAt(int ind) {
    return childs[ind];
}

void Context::destroySubContext() {
    delete root;
}

string Context::getStringConstantById(uint16_t ind) {
    return constantsList[ind];
}

BytecodeFunction *Context::getFunctiontById(uint16_t ind) {
    return functionList[ind];
}

SubContext *Context::getRoot() {
    return root;
}

void SubContext::addVar(Var *var) {
    ownContext->varList.push_back(var);
    variablesById[var->name()] = static_cast<unsigned short>(ownContext->varList.size() - 1);
}

void SubContext::addFun(AstFunction *func) {
    if (parent != nullptr) {
        throw CompileError("Function must be declared in main scope");
    }
    auto *byteCodeFunction = new BytecodeFunction(func);
    uint16_t functionId = static_cast<uint16_t>(functionList.size());
    byteCodeFunction->assignId(functionId);
    functionList.push_back(byteCodeFunction);
    ownContext->functionsById[func->name()] = functionId;
}

SubContext *SubContext::getParent() {
    return parent;
}

SubContext *SubContext::addChild() {
    auto *child = new SubContext(ownContext, this);
    childs.push_back(child);
    return child;
}

Context *SubContext::getVarContext(string name) {
    SubContext *parentWithVariable = findParentWithVariable(name);
    if (parentWithVariable == nullptr) {
        if (ownContext->getParent() != nullptr) {
            return ownContext->getParent()->getVarContext(name);
        } else {
            return nullptr;
        }
    }
    return parentWithVariable;
}

uint16_t SubContext::getVarId(string name) {
    return variablesById[name];
}

SubContext::ChildsIterator *SubContext::childsIterator() {
    return iter;
}

BytecodeFunction *SubContext::getFunction(string name) {
    return ownContext->getFunction(name);
}

uint16_t SubContext::getId() {
    return ownContext->getId();
}

bool SubContext::declareVariable(string name) {
    auto *parentWithVariable = findParentWithVariable(name);
    return parentWithVariable == nullptr;
}

SubContext *SubContext::findParentWithVariable(string name) {
    SubContext *ctx = this;
    while (ctx->variablesById.find(name) == ctx->variablesById.end()) {
        if (ctx->parent == nullptr) {
            return nullptr;
        }
        ctx = ctx->parent;
    }
    return ctx;
}

void StackContext::setInt16(int ind, uint16_t value) {
    (*variables)[ind].i16 = value;
}

void StackContext::setInt64(int ind, int64_t value) {
    (*variables)[ind].i = value;
}

void StackContext::setDouble(int ind, double value) {
    (*variables)[ind].d = value;
}

uint16_t StackContext::getInt16(int ind) {
    return (*variables)[ind].i16;
}

int64_t StackContext::getInt64(int ind) {
    return (*variables)[ind].i;
}

double StackContext::getDouble(int ind) {
    return (*variables)[ind].d;
}

void StackContext::setInt16ToParent(uint16_t parentId, int ind, uint16_t value) {
    auto *parentWithVar = findParentById(parentId, parent);
    parentWithVar->setInt16(ind, value);
}

void StackContext::setInt64ToParent(uint16_t parentId, int ind, int64_t value) {
    auto *parentWithVar = findParentById(parentId, parent);
    parentWithVar->setInt64(ind, value);
}

void StackContext::setDoubleToParent(uint16_t parentId, int ind, double value) {
    auto *parentWithVar = findParentById(parentId, parent);
    parentWithVar->setDouble(ind, value);
}

uint16_t StackContext::getInt16FromParent(uint16_t parentId, int ind) {
    auto *parentWithVar = findParentById(parentId, parent);
    return parentWithVar->getInt16(ind);
}

int64_t StackContext::getInt64FromParent(uint16_t parentId, int ind) {
    auto *parentWithVar = findParentById(parentId, parent);
    return parentWithVar->getInt64(ind);
}

double StackContext::getDoubleFromParent(uint16_t parentId, int ind) {
    auto *parentWithVar = findParentById(parentId, parent);
    return parentWithVar->getDouble(ind);
}

StackContext *StackContext::findParentById(uint16_t id, StackContext *parent) {
    auto *res = parent;
    while (res->id != id) {
        res = res->parent;
    }
    return res;
}

StackContext *StackContext::previousContext() {
    return prev;
}




