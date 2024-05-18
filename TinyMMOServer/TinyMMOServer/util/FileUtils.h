///------------------------------------------------------------------------------------------------
///  FileUtils.h
///  TinyMMOClient
///
///  Created by Alex Koukoulas on 20/09/2023.
///-----------------------------------------------------------------------------------------------

#ifndef FileUtils_h
#define FileUtils_h

///-----------------------------------------------------------------------------------------------

#include "StringUtils.h"

#include <algorithm>
#include <string>
#include <vector>
#include <filesystem>

///-----------------------------------------------------------------------------------------------

namespace fileutils
{

///-----------------------------------------------------------------------------------------------
/// Extracts the file extension from the given file path.
/// @param[in] filePath the input file path.
/// @returns the extension (the string after the dot) of the file path given.
inline std::string GetFileExtension(const std::string& filePath)
{
    std::string pathExt;
    
    auto reverseIter = filePath.rbegin();
    
    while (reverseIter != filePath.rend() && (*reverseIter != '.'))
    {
        pathExt = *reverseIter + pathExt;
        reverseIter++;
    }
    
    return pathExt;
}

///-----------------------------------------------------------------------------------------------
/// Extracts, and returns the file name from the given file path.
/// @param[in] filePath the input file path.
/// @returns the file name (with the extension) from the file path given.
inline std::string GetFileName(const std::string& filePath)
{
    std::string fileName;
    
    auto reverseIter = filePath.rbegin();
    while (reverseIter != filePath.rend() && (*reverseIter != '\\' && *reverseIter != '/'))
    {
        fileName = *reverseIter + fileName;
        reverseIter++;
    }
    
    return fileName;
}

///-----------------------------------------------------------------------------------------------
/// Extracts, and returns the file name without the extension from the given file path.
/// @param[in] filePath the input file path.
/// @returns the file name (without the extension) from the file path given.
inline std::string GetFileNameWithoutExtension(const std::string& filePath)
{
    std::string fileName = "";
    
    auto isRecordingFileName = false;
    auto reverseIter = filePath.rbegin();
    while (reverseIter != filePath.rend() && (*reverseIter != '\\' && *reverseIter != '/'))
    {
        
        if (!isRecordingFileName)
        {
            isRecordingFileName = *reverseIter == '.';
        }
        else
        {
            fileName = *reverseIter + fileName;
        }
        
        reverseIter++;
    }
    
    return fileName;
}

///-----------------------------------------------------------------------------------------------
/// Returns whether or not a filepath points to a directory.
/// @param[in] filePath to assess whether or not is a directory.
/// @returns whether or not a filepath points to a directory.
inline bool IsDirectory(const std::string& filePath)
{
    return std::filesystem::is_directory(filePath);
}

///-----------------------------------------------------------------------------------------------
/// Tries to create a directory from the given path.
/// @param[in] path to attempt to create a directory at.
inline void CreateDirectory(const std::string& path)
{
    std::filesystem::create_directory(path);
}

///-----------------------------------------------------------------------------------------------
/// Returns a vector of filenames (not absolute paths) and directory names in a given directory.
/// @param[in] directory to search in.
/// @returns a vector of filenames & directories found in the given directory.
inline std::vector<std::string> GetAllFilenamesAndFolderNamesInDirectory(const std::string& directory)
{
    std::vector<std::string> fileNames;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        auto fileName = GetFileName(entry.path().string());
        if (strutils::StringSplit(fileName, '.').size() > 1 || IsDirectory(entry.path().string()))
        {
            fileNames.push_back(fileName);
        }
    }
    
    std::sort(fileNames.begin(), fileNames.end());
    return fileNames;
}

///-----------------------------------------------------------------------------------------------

}

///------------------------------------------------------------------------------------------------

#endif /* FileUtils_h */
