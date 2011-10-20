#ifndef VARIABLEMAP_H
#define VARIABLEMAP_H

#include "ast.h"

class VariableMap {
private:
    std::map<mathvm::AstVar const *, int> myVarMap;

public:
    int addVariable(mathvm::AstVar const * var) {
        myVarMap[var] = myVarMap.size();
        return myVarMap.size() - 1;
    }

    uint8_t getId(mathvm::AstVar const * var) {
        return myVarMap[var];
    }

    bool exists (mathvm::AstVar const * v) {
        return myVarMap.find(v) != myVarMap.end();
    }
};

#endif // VARIABLEMAP_H