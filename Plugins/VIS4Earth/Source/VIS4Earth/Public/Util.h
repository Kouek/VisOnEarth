// Author: Kouek Kou

#pragma once

#define VIS4EARTH_COMMA ,

#define VIS4EARTH_DEFINE_VAR_WITH_DEFVAL(Type, Var, DefVal)                                        \
    static inline Type Def##Var = DefVal;                                                          \
    Type Var = Def##Var;

#define VIS4EARTH_THREAD_PER_GROUP_X 16
#define VIS4EARTH_THREAD_PER_GROUP_Y 16
#define VIS4EARTH_THREAD_PER_GROUP_Z 4

#define VIS4EARTH_UI_COMBOBOXSTRING_ADD_OPTIONS(UI, Wdgt)                                          \
    {                                                                                              \
        auto enumClass = StaticEnum<decltype(Wdgt)>();                                             \
        for (int32 i = 0; i < enumClass->GetMaxEnumValue(); ++i)                                   \
            Cast<UComboBoxString>(                                                                 \
                UI->GetWidgetFromName(TEXT("ComboBoxString") TEXT("_") TEXT(#Wdgt)))               \
                ->AddOption(enumClass->GetNameByIndex(i).ToString());                              \
    }

#define VIS4EARTH_UI_ADD_SLOT(ObjTy, Obj, UI, WdgtTy, Wdgt, Signal)                                \
    {                                                                                              \
        auto wdgt = Cast<U##WdgtTy>(UI->GetWidgetFromName(TEXT(#WdgtTy) TEXT("_") TEXT(#Wdgt)));   \
        wdgt->On##Signal.AddDynamic(Obj, &ObjTy::On##WdgtTy##_##Wdgt##Signal);                     \
    }
