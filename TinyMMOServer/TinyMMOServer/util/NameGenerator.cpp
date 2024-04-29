///------------------------------------------------------------------------------------------------
///  NameGenerator.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 29/04/2024
///------------------------------------------------------------------------------------------------

#include "MathUtils.h"
#include "NameGenerator.h"

///------------------------------------------------------------------------------------------------

std::string GenerateName()
{
    static constexpr int NAME_ARRAYS_SIZE = 10;
    
    static std::string prefix[NAME_ARRAYS_SIZE] = {"Nobu", "Hiro", "Kenji", "Taka", "Ryu", "Jin", "Daichi", "Haru", "Kaito", "Toshi"};
    static std::string suffix[NAME_ARRAYS_SIZE] = {"moto", "suke", "hiro", "taro", "kazu", "ichi", "taka", "haru", "yoshi", "o"};
        
    return prefix[math::RandomInt(0, NAME_ARRAYS_SIZE - 1)] + suffix[math::RandomInt(0, NAME_ARRAYS_SIZE - 10)];
}

///------------------------------------------------------------------------------------------------
