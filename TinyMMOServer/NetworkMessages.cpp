///------------------------------------------------------------------------------------------------
///  NetworkMessages.h
///  TinyMMOCommon
///
///  Created by Alex Koukoulas on 30/04/2024
///------------------------------------------------------------------------------------------------

#include "NetworkMessages.h"

///------------------------------------------------------------------------------------------------

namespace networking
{

///------------------------------------------------------------------------------------------------

void PopulateMessageHeader(nlohmann::json& messageJson, const MessageType messageType)
{
    messageJson["messageType"] = static_cast<int>(messageType);
}

///------------------------------------------------------------------------------------------------

bool IsMessageOfType(const nlohmann::json& messageJson, const MessageType messageType)
{
    return messageJson.count("messageType") && static_cast<MessageType>(messageJson.at("messageType").get<int>()) == messageType;
}

///------------------------------------------------------------------------------------------------

MessageType GetMessageType(const nlohmann::json& messageJson)
{
    if (messageJson.count("messageType"))
    {
        return static_cast<MessageType>(messageJson.at("messageType").get<int>());
    }
    
    return MessageType::UNKNOWN_MESSAGE;
}

///------------------------------------------------------------------------------------------------

}
