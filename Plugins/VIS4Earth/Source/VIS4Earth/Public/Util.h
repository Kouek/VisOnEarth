// Author: Kouek Kou

#pragma once

#define VIS4EARTH_COMMA ,

#define VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(Type, Var, DefVal)                                        \
    static inline Type Def##Var = DefVal;                                                          \
    Type Var = Def##Var;

#define VIS4EARTH_THREAD_PER_GROUP_X 16
#define VIS4EARTH_THREAD_PER_GROUP_Y 16
#define VIS4EARTH_THREAD_PER_GROUP_Z 4
