// Author: Kouek Kou

#pragma once

#define VIS4EARTH_COMMA ,

#define VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(Type, Var, DefVal)                                        \
    static inline Type Def##Var = DefVal;                                                          \
    Type Var = Def##Var;
