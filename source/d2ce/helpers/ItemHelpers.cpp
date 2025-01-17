/*
    Diablo II Character Editor
    Copyright (C) 2021-2022 Walter Couto

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
//---------------------------------------------------------------------------

#include "pch.h"
#include "ItemHelpers.h"
#include <map>
#include <bitset>
#include <sstream>
#include <random>
#include <deque>
#include <iterator>
#include <regex>
#include "Item.h"
#include "SkillConstants.h"
#include "DefaultTxtReader.h"
#include <utf8/utf8.h>

//---------------------------------------------------------------------------
namespace d2ce
{
    const std::string s_DefaultLanguage = "enUS";
    std::string s_CurrentLanguage;
    std::set<std::string> s_SupportedLanguages;

    //---------------------------------------------------------------------------
    namespace ItemHelpers
    {
        bool getItemCodev100(std::uint16_t code, std::array<std::uint8_t, 4>& strcode);
        std::uint8_t getItemCodev115(const std::vector<std::uint8_t>& data, size_t startOffset, std::array<std::uint8_t, 4>& strcode);
        void encodeItemCodev115(const std::array<std::uint8_t, 4>& strcode, std::uint64_t& encodedVal, std::uint8_t& numBitsSet);
        std::uint8_t HuffmanDecodeBitString(const std::string& bitstr);

        const ItemType& getInvalidItemTypeHelper();

        void initRunewordData();
        std::string getRunewordNameFromId(std::uint16_t id);
        const d2ce::RunewordType& getRunewordFromId(std::uint16_t id);
        std::vector<d2ce::RunewordType> getPossibleRunewords(const d2ce::Item& item, bool bUseCurrentSocketCount = false, bool bExcludeServerOnly = true);
        std::vector<d2ce::RunewordType> getPossibleRunewords(const d2ce::Item& item, std::uint32_t level, bool bUseCurrentSocketCount = false, bool bExcludeServerOnly = true);

        bool getSetMagicAttribs(std::uint16_t id, std::vector<MagicalAttribute>& attribs, EnumItemVersion version, std::uint16_t gameVersion, std::uint16_t level, std::uint32_t dwb = 0);
        bool getUniqueMagicAttribs(std::uint16_t id, std::vector<MagicalAttribute>& attribs, EnumItemVersion version, std::uint16_t gameVersion, std::uint16_t level, std::uint32_t dwb = 0);
        bool getUniqueQuestMagicAttribs(const std::array<std::uint8_t, 4>& strcode, std::vector<MagicalAttribute>& attribs, EnumItemVersion version, std::uint16_t gameVersion, std::uint16_t level, std::uint32_t dwb = 0);

        std::uint16_t getSetItemId(std::uint16_t id, const std::array<std::uint8_t, 4>& strcode);
        std::uint32_t getSetItemDWBCode(std::uint16_t setItemId);
        std::uint32_t getSetItemDWBCode(std::uint16_t id, const std::array<std::uint8_t, 4>& strcode);
        std::uint8_t generateInferiorQualityId(std::uint16_t level, std::uint32_t dwb = 0);
        bool generateMagicalAffixes(const std::array<std::uint8_t, 4>& strcode, MagicalCachev100& cache, EnumItemVersion version, std::uint16_t gameVersion, std::uint16_t level, std::uint32_t dwb = 0);
        bool generateRareOrCraftedAffixes(const std::array<std::uint8_t, 4>& strcode, RareOrCraftedCachev100& cache, EnumItemVersion version, std::uint16_t gameVersion, std::uint16_t level, std::uint32_t dwb = 0);
        std::uint16_t generateDefenseRating(const std::array<std::uint8_t, 4>& strcode, std::uint32_t dwa = 0);
        std::uint32_t generateDWARandomOffset(std::uint32_t dwa, std::uint16_t numRndCalls);
        std::uint32_t generarateRandomDW();
        std::string getSetNameFromId(std::uint16_t id);
        std::string getSetTCFromId(std::uint16_t id);
        std::uint16_t getSetLevelReqFromId(std::uint16_t id);
        const std::string& getRareNameFromId(std::uint16_t id);
        const std::string& getRareIndexFromId(std::uint16_t id);
        const std::string& getMagicalPrefixFromId(std::uint16_t id);
        const std::string& getMagicalSuffixFromId(std::uint16_t id);
        const std::string& getMagicalPrefixTCFromId(std::uint16_t id);
        const std::string& getMagicalSuffixTCFromId(std::uint16_t id);
        const std::string& getUniqueNameFromId(std::uint16_t id);
        std::uint16_t getIdFromRareIndex(const std::string& rareIndex);
        std::uint16_t getIdFromRareName(const std::string& rareName);
        std::uint16_t getUniqueLevelReqFromId(std::uint16_t id);
        std::string getUniqueTCFromId(std::uint16_t id);
        std::uint16_t getMagicalPrefixLevelReqFromId(std::uint16_t id);
        std::uint16_t getMagicalSuffixLevelReqFromId(std::uint16_t id);

        bool magicalAttributeSorter(const MagicalAttribute& left, const MagicalAttribute& right);
        void checkForRelatedMagicalAttributes(std::vector<MagicalAttribute>& attribs);
        std::string formatMagicalAttributeValue(MagicalAttribute& attrib, std::uint32_t charLevel, size_t idx, const ItemStat& stat);
        bool formatDisplayedMagicalAttribute(MagicalAttribute& attrib, std::uint32_t charLevel);
        void combineMagicalAttribute(std::multimap<size_t, size_t>& itemIndexMap, const std::vector<MagicalAttribute>& newAttribs, std::vector<MagicalAttribute>& attribs);
        bool ProcessNameNode(const Json::Value& node, std::array<char, NAME_LENGTH>& name);
    }

#define read_uint32_bits(start,size) \
    ((*((std::uint32_t *) &data[(start) / 8]) >> ((start) & 7))& (((std::uint32_t)1 << (size)) - 1))

    void ConvertPlaceHolders(std::string& descstr, size_t idxOffset = 0)
    {
        static std::regex re{ "(%([+]?[id]|s))" };
        static std::regex reAlt{ "(%[+]?[0-9])" };

        std::string strFind = "%d";
        size_t strPos = descstr.npos;
        bool useAlt = false;
        std::smatch mr;
        if (std::regex_search(descstr, mr, re))
        {
            strFind = mr[1].str();
            strPos = descstr.find(strFind);
        }
        else if(std::regex_search(descstr, mr, reAlt))
        {
            useAlt = true;
            strFind = mr[1].str();
            strPos = descstr.find(strFind);
        }

        if (strPos != descstr.npos)
        {
            size_t idx = 0;
            while (strPos != descstr.npos)
            {
                std::stringstream ss2;
                if (strFind.find("+") != strFind.npos)
                {
                    ss2 << "+";
                }
                ss2 << "{";
                if (useAlt)
                {
                    ss2 << idx;
                }
                else
                {
                    ss2 << (idx + idxOffset);
                }
                ss2 << "}";
                ++idx;
                descstr.replace(strPos, strFind.size(), ss2.str());

                strPos = descstr.npos;
                if (std::regex_search(descstr, mr, useAlt ? reAlt : re))
                {
                    strFind = mr[1].str();
                    strPos = descstr.find(strFind);
                }
            }

            strFind = "%%";
            strPos = descstr.find(strFind);
            if (strPos != descstr.npos)
            {
                descstr.replace(strPos, strFind.size(), "%");
            }
        }
    }

    void ConvertPlaceHolders(std::string& descstrpos, std::string& descstrneg, bool bGenerateNeg = false, size_t idxOffset = 0)
    {
        ConvertPlaceHolders(descstrpos, idxOffset);
        if (bGenerateNeg)
        {
            ConvertPlaceHolders(descstrneg, idxOffset);
        }
        else
        {
            descstrneg = descstrpos;
        }
    }

    std::string ExtractGenderString(const std::string& strValue, std::string& gender)
    {
        auto pos = strValue.find('[');
        if (pos == strValue.npos)
        {
            return strValue;
        }

        auto pos2 = strValue.find(']', pos);
        if ((pos2 == strValue.npos) || (((pos2 - pos) + 1) != 4))
        {
            return strValue;
        }

        auto start = pos2 + 1;
        auto firstGender = strValue.substr(pos, ((pos2 - pos) + 1));
        if (gender.empty())
        {
            gender.swap(firstGender);
            pos = start;
            pos2 = strValue.find('[', pos);
            if (pos2 == strValue.npos)
            {
                return strValue.substr(pos);
            }

            return strValue.substr(pos, (pos2 - pos));
        }

        pos = strValue.find(gender);
        if (pos == strValue.npos)
        {
            // find possible alternatives
            if (gender == "[fs]" || gender == "[ms]")
            {
                // check if there is a a netural singular
                pos = strValue.find("[nl]");
                if (pos == strValue.npos)
                {
                    // cant find it, so take first
                    pos = start;
                }
            }
            else if (gender == "[fp]" || gender == "[mp]")
            {
                // check if there is a a netural plural
                pos = strValue.find("[pl]");
                if (pos == strValue.npos)
                {
                    // cant find it, so take first
                    pos = start;
                }
            }
            else if (gender == "[nl]")
            {
                if (firstGender == "[fs]" || firstGender == "[ms]")
                {
                    // our first gender option fits our needs
                    pos = start;
                }
                else
                {
                    pos = strValue.find("[ms]");
                    if (pos == strValue.npos)
                    {
                        pos = strValue.find("[fs]");
                        if (pos == strValue.npos)
                        {
                            // cant find it, so take first
                            pos = start;
                        }
                    }
                }
            }
            else if (gender == "[pl]")
            {
                if (firstGender == "[fp]" || firstGender == "[mp]")
                {
                    // our first gender option fits our needs
                    pos = start;
                }
                else
                {
                    pos = strValue.find("[mp]");
                    if (pos == strValue.npos)
                    {
                        pos = strValue.find("[fp]");
                        if (pos == strValue.npos)
                        {
                            // cant find it, so take first
                            pos = start;
                        }
                    }
                }
            }
        }

        if (pos != strValue.npos)
        {
            // found gender
            pos = pos + gender.size();
            pos2 = strValue.find('[', pos);
            if (pos2 == strValue.npos)
            {
                return strValue.substr(pos);
            }

            return strValue.substr(pos, (pos2 - pos));
        }

        return strValue;
    }

    void GetClassAndSkillStrings(std::int64_t value, std::string& strClass, std::string& skillName)
    {
        const auto& skillInfo = CharClassHelper::getSkillById(std::uint16_t(value));
        skillName = skillInfo.name;
        if (skillName.empty())
        {
            skillName = std::to_string(value);
        }

        strClass.clear();
        if (skillInfo.classInfo.has_value())
        {
            strClass = CharClassHelper::getStrClassOnly(skillInfo.classInfo.value().charClass);
        }
    }

    void GetClassAndTabId(std::int64_t value, size_t& classIdx, size_t& tabIdx)
    {
        classIdx = NUM_OF_CLASSES;
        tabIdx = NUM_OF_SKILL_TABS;
        const std::uint32_t numTabNames = NUM_OF_CLASSES * NUM_OF_SKILL_TABS;
        if (value <= numTabNames)
        {
            classIdx = value / NUM_OF_SKILL_TABS;
            tabIdx = value % NUM_OF_SKILL_TABS;
        }
    }

    constexpr size_t MIN_EXPANSION_STRING_IDX = 20000;
    constexpr size_t MIN_PATCH_STRING_IDX = 10000;
    std::map<std::string, size_t> s_StringTxtIdxInfo;
    std::map<size_t, std::map<std::string, std::string>> s_StringTxtInfo;
    std::map<std::string, size_t> s_ExpansionStringTxtIdxInfo;
    std::map<size_t, std::map<std::string, std::string>> s_ExpansionStringTxtInfo;
    std::map<std::string, size_t> s_PatchStringTxtIdxInfo;
    std::map<size_t, std::map<std::string, std::string>> s_PatchStringTxtInfo;
    void StringsTxtDataReader(ITxtDocument& doc)
    {
        if (doc.GetColumnCount() == 0)
        {
            // corrupt
            return;
        }

        auto langs = doc.GetColumnNames();
        for (const auto& lang : langs)
        {
            s_SupportedLanguages.insert(lang);
        }

        size_t numRows = doc.GetRowCount();
        std::string strName;
        std::map<std::string, std::string> stringCols;
        size_t idx = 0;
        for (size_t i = 0; i < numRows; ++i)
        {
            idx = doc.GetRowValues(i, strName, stringCols);
            if (strName.empty() || stringCols.empty() || (idx == MAXSIZE_T))
            {
                continue;
            }

            if (idx >= MIN_EXPANSION_STRING_IDX)
            {
                s_ExpansionStringTxtInfo[idx].swap(stringCols);
                if (strName.compare("x") != 0 && strName.compare("X") != 0)
                {
                    s_ExpansionStringTxtIdxInfo[strName] = idx;
                }
                else
                {
                    // support classic txt files
                    switch (idx)
                    {
                    case 22745: // Ethereal (Cannot be Repaired)
                        s_ExpansionStringTxtIdxInfo["strethereal"] = idx;
                        break;
                    }
                }
            }
            else if (idx >= MIN_PATCH_STRING_IDX)
            {
                s_PatchStringTxtInfo[idx].swap(stringCols);
                if (strName.compare("x") != 0 && strName.compare("X") != 0)
                {
                    s_PatchStringTxtIdxInfo[strName] = idx;
                }
                else
                {
                    // support classic txt files
                    switch (idx)
                    {
                    case 10016:
                        s_PatchStringTxtIdxInfo["strCreateGameHellTextPatch"] = idx;
                        break;
                    case 10017:
                        s_PatchStringTxtIdxInfo["strCreateGameNightmareTextPatch"] = idx;
                        break;
                    case 10018:
                        s_PatchStringTxtIdxInfo["strCreateGameNormalTextPatch"] = idx;
                        break;
                    }
                }
            }
            else
            {
                s_StringTxtInfo[idx].swap(stringCols);
                if (strName.compare("x") != 0 && strName.compare("X") != 0)
                {
                    s_StringTxtIdxInfo[strName] = idx;
                }
                else
                {
                    // support classic txt files
                    switch (idx)
                    {
                    case 5154:
                        s_StringTxtIdxInfo["strCreateGameHellText"] = idx;
                        break;
                    case 5155:
                        s_StringTxtIdxInfo["strCreateGameNightmareText"] = idx;
                        break;
                    case 5156:
                        s_StringTxtIdxInfo["strCreateGameNormalText"] = idx;
                        break;
                    case 5321:
                        s_StringTxtIdxInfo["strName"] = idx;
                        break;
                    case 5322:
                        s_StringTxtIdxInfo["strClass"] = idx;
                        break;
                    }
                }
            }
        }
    }

    const d2ce::ITxtReader* s_pTextReader = nullptr;
    void InitStringsTxtData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_StringTxtInfo.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_SupportedLanguages.clear();
            s_StringTxtIdxInfo.clear();
            s_StringTxtInfo.clear();
            s_ExpansionStringTxtIdxInfo.clear();
            s_ExpansionStringTxtInfo.clear();
            s_PatchStringTxtIdxInfo.clear();
            s_PatchStringTxtInfo.clear();
        }

        bool isClassicTxt = false;
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetPatchStringTxt());
        if(pDoc != nullptr)
        {
            auto& doc = *pDoc;
            if (!isClassicTxt)
            {
                auto colNames = doc.GetColumnNames();
                if (colNames.empty())
                {
                    isClassicTxt = true;
                }
            }

            StringsTxtDataReader(doc);
        }

        pDoc = txtReader.GetExpansionStringTxt();
        if (pDoc != nullptr)
        {
            auto& doc = *pDoc;
            if (!isClassicTxt)
            {
                auto colNames = doc.GetColumnNames();
                if (colNames.empty())
                {
                    isClassicTxt = true;
                }
            }

            StringsTxtDataReader(doc);
        }

        pDoc = txtReader.GetStringTxt();
        if (pDoc != nullptr)
        {
            auto& doc = *pDoc;
            if (!isClassicTxt)
            {
                auto colNames = doc.GetColumnNames();
                if (colNames.empty())
                {
                    isClassicTxt = true;
                }
            }

            StringsTxtDataReader(doc);
        }

        if (isClassicTxt)
        {
            // special cases
            std::uint32_t idx = 11193;
            std::string index("SkillCategoryAm1");
            std::string strValue;
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree6", strValue, "Javelin");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree7", strValue, "and Spear");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryAm2";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree8", strValue, "Passive");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree9", strValue, "and Magic");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryAm3";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree10", strValue, "Bow and");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree11", strValue, "Crossbow");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategorySo1";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree23", strValue, "Cold");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree5", strValue, "Spells");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategorySo2";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree24", strValue, "Lightning");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree5", strValue, "Spells");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategorySo3";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree25", strValue, "Fire");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree5", strValue, "Spells");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryNe1";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                LocalizationHelpers::GetStringTxtValue("StrSklTree16", strValue, "Summoning");
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = strValue;
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryNe2";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree17", strValue, "Poison");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree18", strValue, "and Bone");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryNe3";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                LocalizationHelpers::GetStringTxtValue("StrSklTree19", strValue, "Curses");
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = strValue;
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryPa1";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree12", strValue, "Defensive");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree13", strValue, "Auras");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryPa2";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree14", strValue, "Offensive");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree13", strValue, "Auras");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryPa3";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree15", strValue, "Combat");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree4", strValue, "Skills");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryBa1";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                LocalizationHelpers::GetStringTxtValue("StrSklTree20", strValue, "Warcries");
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = strValue;
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryBa2";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree21", strValue, "Combat");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree22", strValue, "Masteries");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryBa3";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree21", strValue, "Combat");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree4", strValue, "Skills");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryDr1";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                LocalizationHelpers::GetStringTxtValue("StrSklTree29", strValue, "Elemental");
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = strValue;
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryDr2";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree27", strValue, "Shape");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree28", strValue, "Shifting");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryDr3";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                LocalizationHelpers::GetStringTxtValue("StrSklTree26", strValue, "Summoning");
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = strValue;
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryAs1";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree33", strValue, "Martial");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree34", strValue, "Arts");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryAs2";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                std::stringstream ss;
                LocalizationHelpers::GetStringTxtValue("StrSklTree31", strValue, "Shadow");
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("StrSklTree32", strValue, "Disciplines");
                ss << strValue;
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = ss.str();
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            ++idx;
            index = "SkillCategoryAs3";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                LocalizationHelpers::GetStringTxtValue("StrSklTree30", strValue, "Traps");
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = strValue;
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            idx = 11214;
            index = "BeltStorageModifierInfo";
            if (s_PatchStringTxtIdxInfo.find(index) == s_PatchStringTxtIdxInfo.end())
            {
                // support classic txt files
                strValue = "Belt Size: %+d Slots";
                if (s_PatchStringTxtInfo.find(idx) != s_PatchStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_PatchStringTxtInfo.rbegin()->first + 1);
                }
                s_PatchStringTxtInfo[idx][index] = strValue;
                s_PatchStringTxtIdxInfo[index] = idx;
            }

            index = "ItemStats1n";
            auto iterIdx = s_StringTxtIdxInfo.find(index);
            if (iterIdx != s_StringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_StringTxtInfo.find(idx);
                if (iterStr != s_StringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        val += " %d";
                    }
                }
            }

            idx = 23050;
            index = "strItemStatThrowDamageRange";
            if (s_ExpansionStringTxtIdxInfo.find(index) == s_ExpansionStringTxtIdxInfo.end())
            {
                // support classic txt files
                LocalizationHelpers::GetStringTxtValue("ItemStats1n", strValue, "Throw Damage: %d");
                std::stringstream ss;
                ss << strValue;
                ss << " ";
                LocalizationHelpers::GetStringTxtValue("ItemStast1k", strValue, "to");
                ss << strValue;
                ss << " %d";
                strValue = ss.str();

                if (s_ExpansionStringTxtInfo.find(idx) != s_ExpansionStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_ExpansionStringTxtInfo.rbegin()->first + 1);
                }
                s_ExpansionStringTxtInfo[idx][index] = strValue;
                s_ExpansionStringTxtIdxInfo[index] = idx;
            }

            index = "strModEnhancedDamage";
            iterIdx = s_PatchStringTxtIdxInfo.find(index);
            if (iterIdx != s_PatchStringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_PatchStringTxtInfo.find(idx);
                if (iterStr != s_PatchStringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        std::stringstream ss;
                        ss << "%+d%% ";
                        ss << val;
                        val = ss.str();
                    }
                }
            }

            index = "Socketable";
            iterIdx = s_PatchStringTxtIdxInfo.find(index);
            if (iterIdx != s_PatchStringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_PatchStringTxtInfo.find(idx);
                if (iterStr != s_PatchStringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        val += " (%i)";
                    }
                }
            }

            index = "ItemStats1h";
            iterIdx = s_StringTxtIdxInfo.find(index);
            if (iterIdx != s_StringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_StringTxtInfo.find(idx);
                if (iterStr != s_StringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        val += " %d";
                    }
                }
            }

            index = "ItemStats1l";
            iterIdx = s_StringTxtIdxInfo.find(index);
            if (iterIdx != s_StringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_StringTxtInfo.find(idx);
                if (iterStr != s_StringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        std::stringstream ss;
                        ss << val;
                        ss << " %d";
                        LocalizationHelpers::GetStringTxtValue("ItemStast1k", strValue, "to");
                        ss << strValue;
                        ss << " %d";
                        val = ss.str();
                    }
                }
            }

            index = "ItemStats1m";
            iterIdx = s_StringTxtIdxInfo.find(index);
            if (iterIdx != s_StringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_StringTxtInfo.find(idx);
                if (iterStr != s_StringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        std::stringstream ss;
                        ss << val;
                        ss << " %d";
                        LocalizationHelpers::GetStringTxtValue("ItemStast1k", strValue, "to");
                        ss << strValue;
                        ss << " %d";
                        val = ss.str();
                    }
                }
            }

            index = "ItemStats1d";
            iterIdx = s_StringTxtIdxInfo.find(index);
            if (iterIdx != s_StringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_StringTxtInfo.find(idx);
                if (iterStr != s_StringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        std::stringstream ss;
                        ss << val;
                        ss << " %i";
                        LocalizationHelpers::GetStringTxtValue("ItemStats1j", strValue, "of");
                        ss << strValue;
                        ss << " %i";
                        val = ss.str();
                    }
                }
            }

            index = "ItemStats1i";
            iterIdx = s_StringTxtIdxInfo.find(index);
            if (iterIdx != s_StringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_StringTxtInfo.find(idx);
                if (iterStr != s_StringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        std::stringstream ss;
                        ss << val;
                        ss << " %d";
                        LocalizationHelpers::GetStringTxtValue("ItemStats1j", strValue, "of");
                        ss << strValue;
                        ss << " %d";
                        val = ss.str();
                    }
                }
            }

            index = "ItemStats1f";
            iterIdx = s_StringTxtIdxInfo.find(index);
            if (iterIdx != s_StringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_StringTxtInfo.find(idx);
                if (iterStr != s_StringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        val += " %d";
                    }
                }
            }

            index = "ItemStats1e";
            iterIdx = s_StringTxtIdxInfo.find(index);
            if (iterIdx != s_StringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_StringTxtInfo.find(idx);
                if (iterStr != s_StringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        val += " %d";
                    }
                }
            }

            index = "ItemStats1p";
            iterIdx = s_StringTxtIdxInfo.find(index);
            if (iterIdx != s_StringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_StringTxtInfo.find(idx);
                if (iterStr != s_StringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        val += " %d";
                    }
                }
            }

            index = "skilldesc3";
            iterIdx = s_StringTxtIdxInfo.find(index);
            if (iterIdx != s_StringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_StringTxtInfo.find(idx);
                if (iterStr != s_StringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        val += " %d";
                    }
                }
            }

            index = "StrSkill2";
            iterIdx = s_StringTxtIdxInfo.find(index);
            if (iterIdx != s_StringTxtIdxInfo.end())
            {
                idx = std::uint32_t(iterIdx->second);
                auto iterStr = s_StringTxtInfo.find(idx);
                if (iterStr != s_StringTxtInfo.end())
                {
                    auto& val = iterStr->second[index];
                    auto strPos = val.find("%");
                    if (strPos == val.npos)
                    {
                        // support classic txt files
                        val += " %d";
                    }
                }
            }

            idx = 26116;
            index = "TooltipSkillLevelBonus";
            if (s_ExpansionStringTxtIdxInfo.find(index) == s_ExpansionStringTxtIdxInfo.end())
            {
                // support classic txt files
                LocalizationHelpers::GetStringTxtValue("StrSkill2", strValue, "Current Skill Level: %d");
                strValue += " (Base: %d)";
                if (s_ExpansionStringTxtInfo.find(idx) != s_ExpansionStringTxtInfo.end())
                {
                    idx = std::uint32_t(s_ExpansionStringTxtInfo.rbegin()->first + 1);
                }
                s_ExpansionStringTxtInfo[idx][index] = strValue;
                s_ExpansionStringTxtIdxInfo[index] = idx;
            }
        }
    }

    void ProcessStatDescription(std::string& descstrpos, std::string& descstrneg, std::uint8_t descfunc, std::uint16_t descval, const std::string& descstr2)
    {
        std::string strValue;
        std::string strValue2;
        size_t strPos = 0;
        std::string strFind;
        bool bGenerateNeg = descstrpos != descstrneg ? true : false;
        std::stringstream ss;
        std::stringstream ssNeg;
        switch (descfunc)
        {
        case  1:
        case 12:
            switch (descval)
            {
            case 0:
                break;

            case 2:
                ss << descstrpos;
                ss << " +{0}";
                descstrpos = ss.str();

                ssNeg << descstrneg;
                ssNeg << " {0}";
                descstrneg = ssNeg.str();
                break;

            case 1:
            default:
                ss << "+{0} ";
                ss << descstrpos;
                descstrpos = ss.str();

                ssNeg << "{0} ";
                ssNeg << descstrneg;
                descstrneg = ssNeg.str();
                break;
            }
            break;

        case  2: // [value]% [string1]
        case  5: // [value*100/128]% [string1]
            strPos = descstrpos.find("%");
            if (strPos != descstrpos.npos)
            {
                ConvertPlaceHolders(descstrpos, descstrneg, bGenerateNeg);
            }
            else
            {
                // support classic txt files
                switch (descval)
                {
                case 0:
                    break;

                case 2:
                    ss << descstrpos;
                    ss << " {0}%";
                    descstrpos = ss.str();

                    if (bGenerateNeg)
                    {
                        ssNeg << descstrneg;
                        ssNeg << " {0}%";
                        descstrneg = ssNeg.str();
                    }
                    else
                    {
                        descstrneg = descstrpos;
                    }
                    break;

                case 1:
                default:
                    ss << "{0}% ";
                    ss << descstrpos;
                    descstrpos = ss.str();

                    if (bGenerateNeg)
                    {
                        ssNeg << "{0}% ";
                        ssNeg << descstrneg;
                        descstrneg = ssNeg.str();
                    }
                    else
                    {
                        descstrneg = descstrpos;
                    }
                    break;
                }
            }
            break;

        case  3: // [value] [string1]
            switch (descval)
            {
            case 0:
                break;

            case 2:
                ss << descstrpos;
                ss << " {0}";
                descstrpos = ss.str();

                if (bGenerateNeg)
                {
                    ssNeg << descstrneg;
                    ssNeg << " {0}";
                    descstrneg = ssNeg.str();
                }
                else
                {
                    descstrneg = descstrpos;
                }
                break;

            case 1:
            default:
                ss << "{0} ";
                ss << descstrpos;
                descstrpos = ss.str();

                if (bGenerateNeg)
                {
                    ssNeg << "{0} ";
                    ssNeg << descstrneg;
                    descstrneg = ssNeg.str();
                }
                else
                {
                    descstrneg = descstrpos;
                }
                break;
            }
            break;

        case  4: // +[value]% [string1]
            switch (descval)
            {
            case 0:
                break;

            case 2:
                ss << descstrpos;
                ss << " +{0}%";
                descstrpos = ss.str();

                ssNeg << descstrneg;
                ssNeg << " {0}%";
                descstrneg = ssNeg.str();
                break;

            case 1:
            default:
                ss << "+{0}% ";
                ss << descstrpos;
                descstrpos = ss.str();

                ssNeg << "{0}% ";
                ssNeg << descstrneg;
                descstrneg = ssNeg.str();
                break;
            }
            break;

        case  6: // +[value] [string1] [string2]
            switch (descval)
            {
            case 0:
                ss << descstrpos;
                ss << " ";
                ss << descstr2;
                descstrpos = ss.str();

                if (bGenerateNeg)
                {
                    ssNeg << descstrneg;
                    ssNeg << " ";
                    ssNeg << descstr2;
                    descstrneg = ssNeg.str();
                }
                else
                {
                    descstrneg = descstrpos;
                }
                break;

            case 2:
                ss << descstrpos;
                ss << " +{0} ";
                ss << descstr2;
                descstrpos = ss.str();

                ssNeg << descstrneg;
                ssNeg << " {0} ";
                ssNeg << descstr2;
                descstrneg = ssNeg.str();
                break;

            case 1:
            default:
                ss << "+{0} ";
                ss << descstrpos;
                ss << " ";
                ss << descstr2;
                descstrpos = ss.str();

                ssNeg << "{0} ";
                ssNeg << descstrneg;
                ssNeg << " ";
                ssNeg << descstr2;
                descstrneg = ssNeg.str();
                break;
            }
            break;

        case  7: // [value]% [string1] [string2]
        case 10: // [value*100/128]% [string1] [string2]
            switch (descval)
            {
            case 0:
                ss << descstrpos;
                ss << " ";
                ss << descstr2;
                descstrpos = ss.str();

                if (bGenerateNeg)
                {
                    ssNeg << descstrneg;
                    ssNeg << " ";
                    ssNeg << descstr2;
                    descstrneg = ssNeg.str();
                }
                else
                {
                    descstrneg = descstrpos;
                }
                break;

            case 2:
                ss << descstrpos;
                ss << " {0}% ";
                ss << descstr2;
                descstrpos = ss.str();

                if (bGenerateNeg)
                {
                    ssNeg << descstrneg;
                    ssNeg << " {0}% ";
                    ssNeg << descstr2;
                    descstrneg = ssNeg.str();
                }
                else
                {
                    descstrneg = descstrpos;
                }
                break;

            case 1:
            default:
                ss << "{0}% ";
                ss << descstrpos;
                ss << " ";
                ss << descstr2;
                descstrpos = ss.str();

                if (bGenerateNeg)
                {
                    ssNeg << "{0}% ";
                    ssNeg << descstrneg;
                    ssNeg << " ";
                    ssNeg << descstr2;
                    descstrneg = ssNeg.str();
                }
                else
                {
                    descstrneg = descstrpos;
                }
                break;
            }
            break;

        case  8: // +[value]% [string1] [string2]
            switch (descval)
            {
            case 0:
                ss << descstrpos;
                ss << " ";
                ss << descstr2;
                descstrpos = ss.str();

                if (bGenerateNeg)
                {
                    ssNeg << descstrneg;
                    ssNeg << " ";
                    ssNeg << descstr2;
                    descstrneg = ssNeg.str();
                }
                else
                {
                    descstrneg = descstrpos;
                }
                break;

            case 2:
                ss << descstrpos;
                ss << " +{0}% ";
                ss << descstr2;
                descstrpos = ss.str();

                ssNeg << descstrneg;
                ssNeg << " {0}% ";
                ssNeg << descstr2;
                descstrneg = ssNeg.str();
                break;

            case 1:
            default:
                ss << "+{0}% ";
                ss << descstrpos;
                ss << " ";
                ss << descstr2;
                descstrpos = ss.str();

                ssNeg << "{0}% ";
                ssNeg << descstrneg;
                ssNeg << " ";
                ssNeg << descstr2;
                descstrneg = ssNeg.str();
                break;
            }
            break;

        case  9: // [value] [string1] [string2]
            switch (descval)
            {
            case 0:
                ss << descstrpos;
                ss << " ";
                ss << descstr2;
                descstrpos = ss.str();

                if (bGenerateNeg)
                {
                    ssNeg << descstrneg;
                    ssNeg << " ";
                    ssNeg << descstr2;
                    descstrneg = ssNeg.str();
                }
                else
                {
                    descstrneg = descstrpos;
                }
                break;

            case 2:
                ss << descstrpos;
                ss << " {0} ";
                ss << descstr2;
                descstrpos = ss.str();

                if (bGenerateNeg)
                {
                    ssNeg << descstrneg;
                    ssNeg << " {0} ";
                    ssNeg << descstr2;
                    descstrneg = ssNeg.str();
                }
                else
                {
                    descstrneg = descstrpos;
                }
                break;

            case 1:
            default:
                ss << "{0} ";
                ss << descstrpos;
                ss << " ";
                ss << descstr2;
                break;
            }
            descstrpos = ss.str();

            if (bGenerateNeg)
            {
                ssNeg << "{0} ";
                ssNeg << descstrneg;
                ssNeg << " ";
                ssNeg << descstr2;
                descstrneg = ssNeg.str();
            }
            else
            {
                descstrneg = descstrpos;
            }
            break;

        case 11: // 100 / value
            strValue = "ModStre9u";
            LocalizationHelpers::GetStringTxtValue(strValue, descstrpos, "Repairs %d durability in %d seconds");
            strFind = "%d";
            strPos = descstrpos.find(strFind);
            if (strPos != descstrpos.npos)
            {
                descstrpos.replace(strPos, strFind.size(), "1");
                strPos = descstrpos.find(strFind);
                if (strPos != descstrpos.npos)
                {
                    descstrpos.replace(strPos, strFind.size(), "{0}");
                }
            }
            else
            {
                strFind = "%1";
                strPos = descstrpos.find(strFind);
                if (strPos != descstrpos.npos)
                {
                    descstrpos.replace(strPos, strFind.size(), "1");
                    strFind = "%0";
                    strPos = descstrpos.find(strFind);
                    if (strPos != descstrpos.npos)
                    {
                        descstrpos.replace(strPos, strFind.size(), "{0}");
                    }
                }
            }
            descstrneg = descstrpos;
            break;

        case 13: // +[value] to [class] Skill Levels
            descstrpos = "{0}";
            descstrneg = descstrpos;
            break;

        case 14: // +[value] to [skilltab] Skill Levels ([class] Only)
            descstrpos = "{0} {1}";
            descstrneg = descstrpos;
            break;

        case 15: // [chance]% to cast [slvl] [skill] on [event]
            strFind = "%d%%";
            strPos = descstrpos.find(strFind);
            if (strPos != descstrpos.npos)
            {
                descstrpos.replace(strPos, strFind.size(), "{2}%");
                strFind = "%d";
                strPos = descstrpos.find(strFind);
                if (strPos != descstrpos.npos)
                {
                    descstrpos.replace(strPos, strFind.size(), "{0}");
                    strFind = "%s";
                    strPos = descstrpos.find(strFind);
                    if (strPos != descstrpos.npos)
                    {
                        descstrpos.replace(strPos, strFind.size(), "{1}");
                    }
                }

                if (bGenerateNeg)
                {
                    strFind = "%d%%";
                    strPos = descstrneg.find(strFind);
                    if (strPos != descstrneg.npos)
                    {
                        descstrneg.replace(strPos, strFind.size(), "{2}%");
                        strFind = "%d";
                        strPos = descstrneg.find(strFind);
                        if (strPos != descstrneg.npos)
                        {
                            descstrneg.replace(strPos, strFind.size(), "{0}");
                            strFind = "%s";
                            strPos = descstrneg.find(strFind);
                            if (strPos != descstrneg.npos)
                            {
                                descstrneg.replace(strPos, strFind.size(), "{1}");
                            }
                        }
                    }
                }
                else
                {
                    descstrneg = descstrpos;
                }
            }
            else
            {
                strFind = "%0%%";
                strPos = descstrpos.find(strFind);
                if (strPos != descstrpos.npos)
                {
                    descstrpos.replace(strPos, strFind.size(), "{2}%");
                    strFind = "%2";
                    strPos = descstrpos.find(strFind);
                    if (strPos != descstrpos.npos)
                    {
                        descstrpos.replace(strPos, strFind.size(), "{0}");
                        strFind = "%1";
                        strPos = descstrpos.find(strFind);
                        if (strPos != descstrpos.npos)
                        {
                            descstrpos.replace(strPos, strFind.size(), "{1}");
                        }
                    }

                    if (bGenerateNeg)
                    {
                        strFind = "%0%%";
                        strPos = descstrneg.find(strFind);
                        if (strPos != descstrneg.npos)
                        {
                            descstrneg.replace(strPos, strFind.size(), "{2}%");
                            strFind = "%2";
                            strPos = descstrneg.find(strFind);
                            if (strPos != descstrneg.npos)
                            {
                                descstrneg.replace(strPos, strFind.size(), "{0}");
                                strFind = "%1";
                                strPos = descstrneg.find(strFind);
                                if (strPos != descstrneg.npos)
                                {
                                    descstrneg.replace(strPos, strFind.size(), "{1}");
                                }
                            }
                        }
                    }
                    else
                    {
                        descstrneg = descstrpos;
                    }
                }
            }
            break;

        case 16: // Level [sLvl] [skill] Aura When Equipped
            strPos = descstrpos.find("%s");
            if (strPos != descstrpos.npos)
            {
                descstrpos.replace(strPos, 2, "{0}");
                descstrneg = descstrpos;
                ConvertPlaceHolders(descstrpos, descstrneg, bGenerateNeg, 1);
            }
            else
            {
                ConvertPlaceHolders(descstrpos, descstrneg, bGenerateNeg);
            }
            break;

        case 17: // time-based stats were never implemented
        case 18:
            descstrpos.clear();
            descstrneg.clear();
            break;

        case 19: // [value] [string1] [string2]
            ConvertPlaceHolders(descstrpos, descstrneg, bGenerateNeg);
            if (!descstr2.empty())
            {
                descstrpos += " " + descstr2;
                descstrneg += " " + descstr2;
            }
            break;

        case 20: // negative value
            switch (descval)
            {
            case 0:
                break;

            case 2:
                ss << descstrpos;
                ss << " -{0}%";
                descstrpos = ss.str();

                ssNeg << descstrneg;
                ssNeg << " {0}%";
                descstrneg = ssNeg.str();
                break;

            case 1:
            default:
                ss << "-{0}% ";
                ss << descstrpos;
                descstrpos = ss.str();

                ssNeg << "{0}% ";
                ssNeg << descstrneg;
                descstrneg = ssNeg.str();
                break;
            }
            break;

        case 21: // negaive value
            switch (descval)
            {
            case 0:
                break;

            case 2:
                ss << descstrpos;
                ss << " -{0}";
                descstrpos = ss.str();

                ssNeg << descstrneg;
                ssNeg << " {0}";
                descstrneg = ssNeg.str();
                break;

            case 1:
            default:
                ss << "-{0} ";
                ss << descstrpos;
                descstrpos = ss.str();

                ssNeg << "{0} ";
                ssNeg << descstrneg;
                descstrneg = ssNeg.str();
                break;
            }
            break;

        case 22: // vs_montype not supported
        case 23:
            descstrpos.clear();
            descstrneg.clear();
            break;

        case 24: // used for charges
            if (std::count(descstrpos.begin(), descstrpos.end(), '%') > 2)
            {
                ConvertPlaceHolders(descstrpos, descstrneg, false);
            }
            else
            {
                // support classic txt files
                strValue = "ModStre10b";
                LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "Level");
                ss << strValue2;
                ss << " {0} {1} ";

                strPos = descstrpos.find("%d");
                if (strPos != descstrpos.npos)
                {
                    descstrpos.replace(strPos, 2, "{2}");
                    strPos = descstrpos.find("%d");
                    if (strPos != descstrpos.npos)
                    {
                        descstrpos.replace(strPos, 2, "{3}");
                    }
                }
                ss << descstrpos;
                descstrpos = ss.str();
                descstrneg = descstrpos;
            }
            break;

        case 25: // not supported
        case 26:
            descstrpos.clear();
            descstrneg.clear();
            break;

        case 27: // +[value] to [skill] ([class] Only)
        case 28: // +[value] to [skill]
            if (!descstrpos.empty())
            {
                strPos = descstrpos.find("%s");
                if (strPos != descstrpos.npos)
                {
                    descstrpos.replace(strPos, 2, "{0}");
                    descstrneg = descstrpos;
                    ConvertPlaceHolders(descstrpos, descstrneg, false, 1);
                }
                else
                {
                    ConvertPlaceHolders(descstrpos, descstrneg, false);
                }
            }
            else
            {
                ss << "+{1} ";
                strValue = "to";
                LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "to");
                ss << strValue2;
                ss << " {0}";
                descstrpos = ss.str();
                descstrneg = descstrpos;
            }
            break;

        default:
            descstrpos.clear();
            descstrneg.clear();
            break;
        }
    }

    std::map<std::string, std::uint16_t> s_ItemStatsNameMap;
    std::map<d2ce::EnumCharVersion, std::map<std::uint16_t, ItemStat>> s_ItemStatsInfo;
    void InitItemStatsData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemStatsInfo.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemStatsInfo.clear();
            s_ItemStatsNameMap.clear();
        }

        InitStringsTxtData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetItemStatCostTxt());
        auto& doc = *pDoc;
        std::map<std::string, std::uint16_t> itemStatsNameMap;
        std::map<d2ce::EnumCharVersion, std::map<std::uint16_t, ItemStat>> itemStatsInfo;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T nameColumnIdx = doc.GetColumnIdx("Stat");
        if (nameColumnIdx < 0)
        {
            return;
        }

        SSIZE_T idColumnIdx = doc.GetColumnIdx("*ID");
        if (idColumnIdx < 0)
        {
            idColumnIdx = doc.GetColumnIdx("ID");  // support the alternate version of this file format
            if (idColumnIdx < 0)
            {
                return;
            }
        }

        const SSIZE_T encodeColumnIdx = doc.GetColumnIdx("Encode");
        if (encodeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T saveBitsColumnIdx = doc.GetColumnIdx("Save Bits");
        if (saveBitsColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T saveAddColumnIdx = doc.GetColumnIdx("Save Add");
        if (saveAddColumnIdx < 0)
        {
            return;
        }

        SSIZE_T saveBits109ColumnIdx = doc.GetColumnIdx("1.09-Save Bits");
        SSIZE_T saveAdd109ColumnIdx = -1;
        if (saveBits109ColumnIdx >= 0)
        {
            saveAdd109ColumnIdx = doc.GetColumnIdx("1.09-Save Add");
            if (saveAdd109ColumnIdx < 0)
            {
                saveBits109ColumnIdx = -1;
            }
        }

        const SSIZE_T saveParamBitsColumnIdx = doc.GetColumnIdx("Save Param Bits");
        if (saveParamBitsColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T charSaveBitsColumnIdx = doc.GetColumnIdx("CSvBits");
        if (charSaveBitsColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T opColumnIdx = doc.GetColumnIdx("op");
        if (opColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T op_paramColumnIdx = doc.GetColumnIdx("op param");
        if (op_paramColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T op_baseColumnIdx = doc.GetColumnIdx("op base");
        if (op_baseColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T op_stat1ColumnIdx = doc.GetColumnIdx("op stat1");
        if (op_stat1ColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T op_stat2ColumnIdx = doc.GetColumnIdx("op stat2");
        if (op_stat2ColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T op_stat3ColumnIdx = doc.GetColumnIdx("op stat3");
        if (op_stat3ColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T descpriorityColumnIdx = doc.GetColumnIdx("descpriority");
        if (descpriorityColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T descfuncColumnIdx = doc.GetColumnIdx("descfunc");
        if (descfuncColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T descvalColumnIdx = doc.GetColumnIdx("descval");
        if (descvalColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T descstrposColumnIdx = doc.GetColumnIdx("descstrpos");
        if (descstrposColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T descstrnegColumnIdx = doc.GetColumnIdx("descstrneg");
        if (descstrnegColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T descstr2ColumnIdx = doc.GetColumnIdx("descstr2");
        if (descstr2ColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T dgrpColumnIdx = doc.GetColumnIdx("dgrp");
        if (dgrpColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T dgrpfuncColumnIdx = doc.GetColumnIdx("dgrpfunc");
        if (dgrpfuncColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T dgrpvalColumnIdx = doc.GetColumnIdx("dgrpval");
        if (dgrpvalColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T dgrpstrposColumnIdx = doc.GetColumnIdx("dgrpstrpos");
        if (dgrpstrposColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T dgrpstrnegColumnIdx = doc.GetColumnIdx("dgrpstrneg");
        if (dgrpstrnegColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T dgrpstr2ColumnIdx = doc.GetColumnIdx("dgrpstr2");
        if (dgrpstr2ColumnIdx < 0)
        {
            return;
        }

        // get descrition
        std::string strValue;
        std::string strValue2;
        std::string descstrpos;
        std::string descstrneg;
        std::string descstr2;
        std::uint16_t id = 0;
        for (size_t i = 0; i < numRows; ++i)
        {
            id = doc.GetCellUInt16(idColumnIdx, i);
            if (itemStatsInfo[d2ce::EnumCharVersion::v100].find(id) != itemStatsInfo[d2ce::EnumCharVersion::v100].end())
            {
                // skip
                continue;
            }

            auto& newValue = itemStatsInfo[d2ce::EnumCharVersion::v100][id];
            newValue.id = id;
            newValue.name = doc.GetCellString(nameColumnIdx, i);
            itemStatsNameMap[newValue.name] = id;
            if (newValue.name.compare("item_maxdamage_percent") == 0 ||
                newValue.name.compare("firemindam") == 0 ||
                newValue.name.compare("lightmindam") == 0 ||
                newValue.name.compare("magicmindam") == 0 ||
                newValue.name.compare("coldmindam") == 0 ||
                newValue.name.compare("coldmaxdam") == 0 ||
                newValue.name.compare("poisonmindam") == 0 ||
                newValue.name.compare("poisonmaxdam") == 0)
            {
                newValue.nextInChain = newValue.id + 1;

                if (newValue.name.compare("item_maxdamage_percent") == 0)
                {
                    strValue = "strModEnhancedDamage";
                    LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "%+d%% Enhanced Damage");
                    ConvertPlaceHolders(strValue2);
                    newValue.descRange = strValue2;
                    newValue.descNoRange = newValue.descRange;
                }
                else if (newValue.name.compare("firemindam") == 0)
                {
                    strValue = "strModFireDamageRange";
                    LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "Adds %d-%d fire damage");
                    ConvertPlaceHolders(strValue2);
                    newValue.descRange = strValue2;

                    strValue = "strModFireDamage";
                    LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "%+d fire damage");
                    ConvertPlaceHolders(strValue2, 1);
                    newValue.descNoRange = strValue2;
                }
                else if (newValue.name.compare("lightmindam") == 0)
                {
                    strValue = "strModLightningDamageRange";
                    LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "Adds %d-%d lightning damage");
                    ConvertPlaceHolders(strValue2);
                    newValue.descRange = strValue2;

                    strValue = "strModLightningDamage";
                    LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "%+d lightning damage");
                    ConvertPlaceHolders(strValue2, 1);
                    newValue.descNoRange = strValue2;
                }
                else if (newValue.name.compare("magicmindam") == 0)
                {
                    strValue = "strModMagicDamageRange";
                    LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "Adds %d-%d magic damage");
                    ConvertPlaceHolders(strValue2);
                    newValue.descRange = strValue2;

                    strValue = "strModMagicDamage";
                    LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "%+d magic damage");
                    ConvertPlaceHolders(strValue2, 1);
                    newValue.descNoRange = strValue2;
                }
                else if (newValue.name.compare("coldmindam") == 0)
                {
                    strValue = "strModColdDamageRange";
                    LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "Adds %d-%d cold damage");
                    ConvertPlaceHolders(strValue2);
                    newValue.descRange = strValue2;

                    strValue = "strModColdDamage";
                    LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "%+d cold damage");
                    ConvertPlaceHolders(strValue2, 1);
                    newValue.descNoRange = strValue2;
                }
                else if (newValue.name.compare("poisonmindam") == 0)
                {
                    strValue = "strModPoisonDamageRange";
                    LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "Adds %d-%d poison damage over %d seconds");
                    ConvertPlaceHolders(strValue2);
                    newValue.descRange = strValue2;

                    strValue = "strModPoisonDamage";
                    LocalizationHelpers::GetStringTxtValue(strValue, strValue2, "%+d poison damage over %d seconds");
                    ConvertPlaceHolders(strValue2, 1);
                    newValue.descNoRange = strValue2;
                }
            }

            newValue.encode = doc.GetCellString(encodeColumnIdx, i).empty() ? 0ui8 : (std::uint8_t)doc.GetCellUInt16(encodeColumnIdx, i);
            newValue.saveBits = doc.GetCellString(saveBitsColumnIdx, i).empty() ? 0ui16 : doc.GetCellUInt16(saveBitsColumnIdx, i);
            newValue.saveAdd = doc.GetCellString(saveAddColumnIdx, i).empty() ? 0ui16 : doc.GetCellUInt16(saveAddColumnIdx, i);
            newValue.saveParamBits = doc.GetCellString(saveParamBitsColumnIdx, i).empty() ? 0ui16 : doc.GetCellUInt16(saveParamBitsColumnIdx, i);
            newValue.charSaveBits = doc.GetCellString(charSaveBitsColumnIdx, i).empty() ? 0ui16 : doc.GetCellUInt16(charSaveBitsColumnIdx, i);

            //op values
            strValue = doc.GetCellString(opColumnIdx, i);
            if (!strValue.empty())
            {
                newValue.opAttribs.op = (std::uint8_t)doc.GetCellUInt16(opColumnIdx, i);
                newValue.opAttribs.op_param = doc.GetCellString(op_paramColumnIdx, i).empty() ? 0ui8 : (std::uint8_t)doc.GetCellUInt16(op_paramColumnIdx, i);
                newValue.opAttribs.op_base = doc.GetCellString(op_baseColumnIdx, i);
                strValue = doc.GetCellString(op_stat1ColumnIdx, i);
                if (!strValue.empty())
                {
                    newValue.opAttribs.op_stats.push_back(strValue);
                    strValue = doc.GetCellString(op_stat2ColumnIdx, i);
                    if (!strValue.empty())
                    {
                        newValue.opAttribs.op_stats.push_back(strValue);
                        strValue = doc.GetCellString(op_stat3ColumnIdx, i);
                        if (!strValue.empty())
                        {
                            newValue.opAttribs.op_stats.push_back(strValue);
                        }
                    }
                }
            }

            strValue = doc.GetCellString(descpriorityColumnIdx, i);
            if (!strValue.empty())
            {
                newValue.descPriority = (std::uint8_t)doc.GetCellUInt16(descpriorityColumnIdx, i);
            }

            strValue = doc.GetCellString(descfuncColumnIdx, i);
            if (!strValue.empty())
            {
                newValue.descFunc = (std::uint8_t)doc.GetCellUInt16(descfuncColumnIdx, i);
            }

            strValue = doc.GetCellString(descvalColumnIdx, i);
            if (!strValue.empty())
            {
                newValue.descVal = (std::uint8_t)doc.GetCellUInt16(descvalColumnIdx, i);
            }

            LocalizationHelpers::GetStringTxtValue(doc.GetCellString(descstrposColumnIdx, i), descstrpos);
            LocalizationHelpers::GetStringTxtValue(doc.GetCellString(descstrnegColumnIdx, i), descstrneg);
            LocalizationHelpers::GetStringTxtValue(doc.GetCellString(descstr2ColumnIdx, i), descstr2);

            // base description
            ProcessStatDescription(descstrpos, descstrneg, newValue.descFunc, newValue.descVal, descstr2);
            newValue.desc = descstrpos;
            newValue.descNeg = descstrneg;

            strValue = doc.GetCellString(dgrpColumnIdx, i);
            if (!strValue.empty() && (strValue != "0"))
            {
                LocalizationHelpers::GetStringTxtValue(doc.GetCellString(dgrpstrposColumnIdx, i), descstrpos);
                LocalizationHelpers::GetStringTxtValue(doc.GetCellString(dgrpstrnegColumnIdx, i), descstrneg);
                LocalizationHelpers::GetStringTxtValue(doc.GetCellString(dgrpstr2ColumnIdx, i), descstr2);

                std::uint8_t descFunc = 0;
                std::uint8_t descVal = 0; 
                strValue = doc.GetCellString(dgrpfuncColumnIdx, i);
                if (!strValue.empty())
                {
                    descFunc = (std::uint8_t)doc.GetCellUInt16(dgrpfuncColumnIdx, i);
                }

                strValue = doc.GetCellString(dgrpvalColumnIdx, i);
                if (!strValue.empty())
                {
                    descVal = (std::uint8_t)doc.GetCellUInt16(dgrpvalColumnIdx, i);
                }

                ProcessStatDescription(descstrpos, descstrneg, descFunc, descVal, descstr2);
                newValue.descGrp = descstrpos;
                newValue.descGrpNeg = descstrneg;
            }

            if (!newValue.descRange.empty())
            {
                newValue.desc = newValue.descRange;
                newValue.descNeg = newValue.desc;
            }

            if (saveBits109ColumnIdx >= 0)
            {
                auto& newValue2 = itemStatsInfo[d2ce::EnumCharVersion::v109][id];
                newValue2 = newValue;
                newValue.saveBits = doc.GetCellString(saveBits109ColumnIdx, i).empty() ? 0ui16 : doc.GetCellUInt16(saveBits109ColumnIdx, i);
                newValue.saveAdd = doc.GetCellString(saveAdd109ColumnIdx, i).empty() ? 0ui16 : doc.GetCellUInt16(saveAdd109ColumnIdx, i);
            }
        }

        s_ItemStatsInfo.swap(itemStatsInfo);
        s_ItemStatsNameMap.swap(itemStatsNameMap);
    }

    struct ItemCategoryType
    {
        std::string code; // the ID pointer of this ItemType (iType), this pointer is used in many txt files (armor.txt, cubemain.txt, misc.txt, skills.txt, treasureclassex.txt, weapons.txt)
        std::string name; // internal name
        std::vector<std::string> subCodes; // parent iTypes
        std::optional<std::string> quiverCode; // What quiver does this category shoot

        bool beltable = false; // Can this iType be placed in your characters belt slots?

        // Maximum sockets mapped by item level threshold (default 1, 25 and 40)
        std::map<std::uint16_t, std::uint8_t> max_sockets;

        // What body locations can this item be equipped
        std::vector<EnumEquippedId> bodyLocations;
    };

    std::map<std::string, ItemCategoryType> s_ItemCategoryType;
    void InitItemTypesData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemCategoryType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemCategoryType.clear();
        }

        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetItemTypesTxt());
        auto& doc = *pDoc;
        std::map<std::string, ItemCategoryType> itemCategoryType;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T nameColumnIdx = doc.GetColumnIdx("ItemType");
        if (nameColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T codeColumnIdx = doc.GetColumnIdx("Code");
        if (codeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T subCode1ColumnIdx = doc.GetColumnIdx("Equiv1");
        if (subCode1ColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T subCode2ColumnIdx = doc.GetColumnIdx("Equiv2");
        if (subCode2ColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T beltableColumnIdx = doc.GetColumnIdx("Beltable");
        if (beltableColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T bodyColumnIdx = doc.GetColumnIdx("Body");
        if (bodyColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T bodyLoc1ColumnIdx = doc.GetColumnIdx("BodyLoc1");
        if (bodyLoc1ColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T bodyLoc2ColumnIdx = doc.GetColumnIdx("BodyLoc2");
        if (bodyLoc2ColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T shootsColumnIdx = doc.GetColumnIdx("Shoots");
        if (shootsColumnIdx < 0)
        {
            return;
        }

        size_t maxSocketParamSize = 3;
        static std::uint16_t maxLevelThresholds[] = { 1ui16, 25ui16, 40ui16 };
        std::vector<SSIZE_T> maxSockets(maxSocketParamSize, -1);
        std::vector<SSIZE_T> maxSocketsLevelThreshold(maxSocketParamSize, -1);
        for (size_t idx = 1; idx <= maxSocketParamSize; ++idx)
        {
            maxSockets[idx - 1] = doc.GetColumnIdx("MaxSockets" + std::to_string(idx));
            if (maxSockets[idx - 1] < 0)
            {
                maxSockets[idx - 1] = doc.GetColumnIdx("MaxSock" + std::to_string(maxLevelThresholds[idx - 1])); // support the alternate version of this file format
                if (maxSockets[idx - 1] < 0)
                {
                    maxSocketParamSize = idx - 1;
                    maxSockets.resize(maxSocketParamSize);
                    maxSocketsLevelThreshold.resize(maxSocketParamSize);
                    break;
                }
                else
                {
                    maxSocketsLevelThreshold[idx - 1] = -1;
                }
            }
            else
            {
                maxSocketsLevelThreshold[idx - 1] = doc.GetColumnIdx("MaxSocketsLevelThreshold" + std::to_string(idx));
                if (maxSocketsLevelThreshold[idx - 1] < 0)
                {
                    maxSocketParamSize = idx - 1;
                    maxSockets.resize(maxSocketParamSize);
                    maxSocketsLevelThreshold.resize(maxSocketParamSize);
                    break;
                }
            }
        }

        std::uint16_t levelThreshold = 1;
        std::string strValue;
        for (size_t i = 0; i < numRows; ++i)
        {
            strValue = doc.GetCellString(codeColumnIdx, i);
            if (strValue.empty())
            {
                // skip
                continue;
            }

            auto& itemType = itemCategoryType[strValue];
            itemType.code = strValue;
            itemType.name = doc.GetCellString(nameColumnIdx, i);

            strValue = doc.GetCellString(subCode1ColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.subCodes.push_back(strValue);
                strValue = doc.GetCellString(subCode2ColumnIdx, i);
                if (!strValue.empty())
                {
                    itemType.subCodes.push_back(strValue);
                }
            }
            
            strValue = doc.GetCellString(beltableColumnIdx, i);
            if (!strValue.empty() && (strValue != "0"))
            {
                itemType.beltable = true;
            }

            strValue = doc.GetCellString(bodyColumnIdx, i);
            if (!strValue.empty() && (strValue != "0"))
            {
                static std::map<std::string, EnumEquippedId> s_equipMap = {
                    {"head", EnumEquippedId::HEAD}, {"neck", EnumEquippedId::NECK}, {"tors", EnumEquippedId::TORSO},
                    {"rarm", EnumEquippedId::RIGHT_ARM}, {"larm", EnumEquippedId::LEFT_ARM}, 
                    {"rrin", EnumEquippedId::RIGHT_RING}, {"lrin", EnumEquippedId::LEFT_RING}, {"belt", EnumEquippedId::BELT}, {"feet", EnumEquippedId::FEET},
                    {"glov", EnumEquippedId::GLOVES}
                };

                strValue = doc.GetCellString(bodyLoc1ColumnIdx, i);
                if (!strValue.empty())
                {
                    auto iter = s_equipMap.find(strValue);
                    if (iter != s_equipMap.end())
                    {
                        itemType.bodyLocations.push_back(iter->second);
                    }

                    strValue = doc.GetCellString(bodyLoc2ColumnIdx, i);
                    if (!strValue.empty() )
                    {
                        iter = s_equipMap.find(strValue);
                        if (iter != s_equipMap.end())
                        {
                            if (std::find(itemType.bodyLocations.begin(), itemType.bodyLocations.end(), iter->second) == itemType.bodyLocations.end())
                            {
                                itemType.bodyLocations.push_back(iter->second);
                            }
                        }
                    }
                }
            }

            for (size_t idx = 0; idx < maxSocketParamSize; ++idx)
            {
                strValue = doc.GetCellString(maxSockets[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                levelThreshold = maxLevelThresholds[idx];
                if (maxSocketsLevelThreshold[idx] >= 0)
                {
                    strValue = doc.GetCellString(maxSocketsLevelThreshold[idx], i);
                    if (!strValue.empty())
                    {
                        levelThreshold = doc.GetCellUInt16(maxSocketsLevelThreshold[idx], i);
                    }
                }

                itemType.max_sockets[levelThreshold] = std::uint8_t(doc.GetCellUInt16(maxSockets[idx], i));
            }

            if (std::find(itemType.subCodes.begin(), itemType.subCodes.end(), "misl") != itemType.subCodes.end())
            {
                strValue = doc.GetCellString(shootsColumnIdx, i);
                if (!strValue.empty())
                {
                    itemType.quiverCode = strValue;
                }
            }
        }

        s_ItemCategoryType.swap(itemCategoryType);
    }

    const ItemCategoryType& GetItemCategory(const std::string code)
    {
        auto iter = s_ItemCategoryType.find(code);
        if (iter == s_ItemCategoryType.end())
        {
            static ItemCategoryType badValue;
            return badValue;
        }

        return iter->second;
    }

    struct ItemPropertiesParamType
    {
        std::string set;  // Parameter to the property function, if the function can use it
        std::string val;  // Parameter to the property function, if the function can use it
        std::string func; // Function used to assign a value to a property
        std::string stat; // Stat applied by the property (see ItemStatCost.txt), if the function uses it.
    };

    struct ItemPropertiesType
    {
        std::string code;                                // property identifier to use in other txt files anywhere a property is applied
                                                         // (e.g. MagicPrefix, MagicSuffix, Automagic, MonProp, Gems, Runes, Sets, SetItems, UniqueItems, Cubemain).
        bool active = false;                             // is property active?
        std::uint16_t version = 0;                       // 0 = Classic D2, 100 = Expansion
        std::vector<ItemPropertiesParamType> parameters; // Parameters for the property function
    };

    std::map<std::string, ItemPropertiesType> s_ItemPropertiesType;
    void InitItemPropertiesData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemPropertiesType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemPropertiesType.clear();
        }

        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetPropertiesTxt());
        auto& doc = *pDoc;
        std::map<std::string, ItemPropertiesType> itemPropertiesType;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T codeColumnIdx = doc.GetColumnIdx("code");
        if (codeColumnIdx < 0)
        {
            return;
        }

        SSIZE_T doneColumnIdx = doc.GetColumnIdx("*Enabled");
        if (doneColumnIdx < 0)
        {
            doneColumnIdx = doc.GetColumnIdx("*done"); // support the alternate version of this file format
            if (doneColumnIdx < 0)
            {
                return;
            };
        }

        const size_t paramSize = 7;
        std::vector<SSIZE_T> setColumnIdx(paramSize, -1);
        std::vector<SSIZE_T> valColumnIdx(paramSize, -1);
        std::vector<SSIZE_T> funcColumnIdx(paramSize, -1);
        std::vector<SSIZE_T> statColumnIdx(paramSize, -1);
        for (size_t idx = 1; idx <= paramSize; ++idx)
        {
            setColumnIdx[idx - 1] = doc.GetColumnIdx("set" + std::to_string(idx));
            if (setColumnIdx[idx - 1] < 0)
            {
                return;
            }

            valColumnIdx[idx - 1] = doc.GetColumnIdx("val" + std::to_string(idx));
            if (valColumnIdx[idx - 1] < 0)
            {
                return;
            }

            funcColumnIdx[idx - 1] = doc.GetColumnIdx("func" + std::to_string(idx));
            if (funcColumnIdx[idx - 1] < 0)
            {
                return;
            }

            statColumnIdx[idx - 1] = doc.GetColumnIdx("stat" + std::to_string(idx));
            if (statColumnIdx[idx - 1] < 0)
            {
                return;
            }
        }

        std::uint16_t version = 0;
        std::string strValue;
        for (size_t i = 0; i < numRows; ++i)
        {
            strValue = doc.GetCellString(codeColumnIdx, i);
            if (strValue.empty())
            {
                // skip
                continue;
            }

            if (strValue == "Expansion")
            {
                // skip
                version = 100;
                continue;
            }

            auto& itemProp = itemPropertiesType[strValue];
            itemProp.code = strValue;
            itemProp.version = version;

            strValue = doc.GetCellString(doneColumnIdx, i);
            if (!strValue.empty() && strValue != "0")
            {
                itemProp.active = true;
            }

            for (size_t idx = 0; idx < paramSize; ++idx)
            {
                bool bHasValue = false;
                ItemPropertiesParamType params;
                strValue = doc.GetCellString(setColumnIdx[idx], i);
                if (!strValue.empty())
                {
                    params.set = strValue;
                    bHasValue = true;
                }

                strValue = doc.GetCellString(valColumnIdx[idx], i);
                if (!strValue.empty())
                {
                    params.val = strValue;
                    bHasValue = true;
                }

                strValue = doc.GetCellString(funcColumnIdx[idx], i);
                if (!strValue.empty())
                {
                    params.func = strValue;
                    bHasValue = true;
                }

                strValue = doc.GetCellString(statColumnIdx[idx], i);
                if (!strValue.empty())
                {
                    params.stat = strValue;
                    bHasValue = true;
                }
                else
                {
                    if (!params.func.empty())
                    {
                        auto func = std::uint16_t(std::atoi(params.func.c_str()));
                        switch (func)
                        {
                        case 5:
                            params.stat = "mindamage";
                            break;

                        case 6:
                            params.stat = "maxdamage";
                            break;

                        case 7:
                            params.stat = "damagepercent";
                            break;

                        case 20:
                            params.stat = "item_indesctructible";
                            break;
                        }
                    }
                }

                if (!bHasValue)
                {
                    break;
                }

                itemProp.parameters.push_back(params);
            }
        }

        s_ItemPropertiesType.swap(itemPropertiesType);
    }

    std::vector<std::uint16_t> s_ItemBeltSlots;
    void InitItemBeltSlots(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemBeltSlots.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemBeltSlots.clear();
        }

        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetBeltsTxt());
        auto& doc = *pDoc;
        std::vector<std::uint16_t> itemBeltSlots;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T numboxesColumnIdx = doc.GetColumnIdx("numboxes");
        if (numboxesColumnIdx < 0)
        {
            return;
        }

        std::string strValue;
        for (size_t i = 0; i < numRows; ++i)
        {
            strValue = doc.GetCellString(numboxesColumnIdx, i);
            if (strValue.empty())
            {
                // skip
                continue;
            }

            itemBeltSlots.push_back(doc.GetCellUInt16(numboxesColumnIdx, i));
        }

        s_ItemBeltSlots.swap(itemBeltSlots);
    }

    ItemType s_invalidItemType;
    std::uint16_t s_ItemCurTypeCodeIdx_v100 = 0;
    std::map<std::uint16_t, std::string> s_ItemTypeCodes_v100;
    std::map<std::string, ItemType> s_ItemWeaponType;
    const ItemType& GetWeaponItemType(const std::string& code)
    {
        auto iter = s_ItemWeaponType.find(code);
        if (iter != s_ItemWeaponType.end())
        {
            return iter->second;
        }

        return s_invalidItemType;
    }

    void InitItemWeaponTypesData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemWeaponType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemWeaponType.clear();
        }

        InitItemBeltSlots(txtReader);
        InitItemTypesData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetWeaponsTxt());
        auto& doc = *pDoc;
        std::map<std::string, ItemType> itemWeaponType;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T nameColumnIdx = doc.GetColumnIdx("name");
        if (nameColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T versionColumnIdx = doc.GetColumnIdx("version");
        if (versionColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T compactsaveColumnIdx = doc.GetColumnIdx("compactsave");
        if (compactsaveColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T codeColumnIdx = doc.GetColumnIdx("code");
        if (codeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T namestrColumnIdx = doc.GetColumnIdx("namestr");
        if (namestrColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T durabilityColumnIdx = doc.GetColumnIdx("durability");
        if (durabilityColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T nodurabilityColumnIdx = doc.GetColumnIdx("nodurability");
        if (nodurabilityColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T typeColumnIdx = doc.GetColumnIdx("type");
        if (typeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T type2ColumnIdx = doc.GetColumnIdx("type2");
        if (type2ColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T mindamColumnIdx = doc.GetColumnIdx("mindam");
        if (mindamColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T maxdamColumnIdx = doc.GetColumnIdx("maxdam");
        if (maxdamColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T oneOrTwoHandedColumnIdx = doc.GetColumnIdx("1or2handed");
        if (oneOrTwoHandedColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T TwoHandedColumnIdx = doc.GetColumnIdx("2handed");
        if (TwoHandedColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T TwoHandmindamColumnIdx = doc.GetColumnIdx("2handmindam");
        if (TwoHandmindamColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T TwoHandmaxdamColumnIdx = doc.GetColumnIdx("2handmaxdam");
        if (TwoHandmaxdamColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T minmisdamColumnIdx = doc.GetColumnIdx("minmisdam");
        if (minmisdamColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T maxmisdamColumnIdx = doc.GetColumnIdx("maxmisdam");
        if (maxmisdamColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T stackableColumnIdx = doc.GetColumnIdx("stackable");
        if (stackableColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T minstackColumnIdx = doc.GetColumnIdx("minstack");
        if (minstackColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T maxstackColumnIdx = doc.GetColumnIdx("maxstack");
        if (maxstackColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T levelColumnIdx = doc.GetColumnIdx("level");
        if (levelColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T magiclvlColumnIdx = doc.GetColumnIdx("magic lvl");
        if (magiclvlColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T reqstrColumnIdx = doc.GetColumnIdx("reqstr");
        if (reqstrColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T reqdexColumnIdx = doc.GetColumnIdx("reqdex");
        if (reqdexColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T levelreqColumnIdx = doc.GetColumnIdx("levelreq");
        if (levelreqColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invwidthColumnIdx = doc.GetColumnIdx("invwidth");
        if (invwidthColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invheightColumnIdx = doc.GetColumnIdx("invheight");
        if (invheightColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invfileColumnIdx = doc.GetColumnIdx("invfile");
        if (invfileColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invTransColumnIdx = doc.GetColumnIdx("InvTrans");
        if (invTransColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T questColumnIdx = doc.GetColumnIdx("quest");
        if (questColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T gemsocketsColumnIdx = doc.GetColumnIdx("gemsockets");
        if (gemsocketsColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T gemapplytypeColumnIdx = doc.GetColumnIdx("gemapplytype");
        if (gemapplytypeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T nameableColumnIdx = doc.GetColumnIdx("Nameable");
        if (nameableColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T skipNameColumnIdx = doc.GetColumnIdx("SkipName");
        if (skipNameColumnIdx < 0)
        {
            return;
        }

        std::string strValue;
        std::string name;
        std::string code;
        std::deque<std::string> codes;
        for (size_t i = 0; i < numRows; ++i)
        {
            codes.clear();
            code = doc.GetCellString(codeColumnIdx, i);
            if (code.empty())
            {
                // skip
                continue;
            }

            auto& itemType = itemWeaponType[code];
            itemType.code = code;
            itemType.name = doc.GetCellString(nameColumnIdx, i);
            strValue = doc.GetCellString(namestrColumnIdx, i);
            if (strValue.empty())
            {
                if (LocalizationHelpers::GetStringTxtValue(code, name))
                {
                    itemType.name = name;
                }
            }
            else if (LocalizationHelpers::GetStringTxtValue(strValue, name))
            {
                itemType.name = name;
            }

            strValue = doc.GetCellString(versionColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.version = doc.GetCellUInt16(versionColumnIdx, i);
                if (itemType.version == 0)
                {
                    itemType.code_v100 = s_ItemCurTypeCodeIdx_v100;
                    ++s_ItemCurTypeCodeIdx_v100;
                    s_ItemTypeCodes_v100[itemType.code_v100] = itemType.code;
                }
            }

            strValue = doc.GetCellString(compactsaveColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.compactsave = doc.GetCellUInt16(compactsaveColumnIdx, i) != 0 ? true : false;
            }

            strValue = doc.GetCellString(durabilityColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.durability.Base = doc.GetCellUInt16(durabilityColumnIdx, i);
                if (itemType.durability.Base != 0)
                {
                    strValue = doc.GetCellString(nodurabilityColumnIdx, i);
                    if (strValue.empty() || (doc.GetCellUInt16(nodurabilityColumnIdx, i) == 0))
                    {
                        itemType.durability.Max = itemType.durability.Base;
                    }
                }
            }

            strValue = doc.GetCellString(typeColumnIdx, i);
            if (!strValue.empty())
            {
                codes.push_back(strValue);
            }

            strValue = doc.GetCellString(mindamColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.dam.OneHanded.Min = doc.GetCellUInt16(mindamColumnIdx, i);
                itemType.dam.OneHanded.Max = itemType.dam.OneHanded.Min;
            }

            strValue = doc.GetCellString(maxdamColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.dam.OneHanded.Max = doc.GetCellUInt16(maxdamColumnIdx, i);
            }

            itemType.dam.bOneOrTwoHanded = false;
            strValue = doc.GetCellString(oneOrTwoHandedColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.dam.bOneOrTwoHanded = strValue == "1" ? true : false;
            }

            itemType.dam.bTwoHanded = itemType.dam.bOneOrTwoHanded;
            strValue = doc.GetCellString(TwoHandedColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.dam.bTwoHanded = strValue == "1" ? true : false;
            }

            if (itemType.dam.bOneOrTwoHanded || itemType.dam.bTwoHanded)
            {
                strValue = doc.GetCellString(TwoHandmindamColumnIdx, i);
                if (!strValue.empty())
                {
                    itemType.dam.TwoHanded.Min = doc.GetCellUInt16(TwoHandmindamColumnIdx, i);
                    itemType.dam.TwoHanded.Max = itemType.dam.TwoHanded.Min;
                }

                strValue = doc.GetCellString(TwoHandmaxdamColumnIdx, i);
                if (!strValue.empty())
                {
                    itemType.dam.TwoHanded.Max = doc.GetCellUInt16(TwoHandmaxdamColumnIdx, i);
                }
            }
            else
            {
                strValue = doc.GetCellString(minmisdamColumnIdx, i);
                if (!strValue.empty())
                {
                    itemType.dam.Missile.Min = doc.GetCellUInt16(minmisdamColumnIdx, i);
                    itemType.dam.Missile.Max = itemType.dam.Missile.Min;
                }

                strValue = doc.GetCellString(maxmisdamColumnIdx, i);
                if (!strValue.empty())
                {
                    itemType.dam.Missile.Max = doc.GetCellUInt16(maxmisdamColumnIdx, i);
                }
            }

            strValue = doc.GetCellString(stackableColumnIdx, i);
            if (!strValue.empty())
            {
                if (doc.GetCellUInt16(stackableColumnIdx, i) != 0)
                {
                    strValue = doc.GetCellString(minstackColumnIdx, i);
                    if (!strValue.empty())
                    {
                        itemType.stackable.Min = doc.GetCellUInt32(minstackColumnIdx, i) % 512ui32;
                    }

                    strValue = doc.GetCellString(maxstackColumnIdx, i);
                    if (!strValue.empty())
                    {
                        itemType.stackable.Max = std::max(itemType.stackable.Min, doc.GetCellUInt32(maxstackColumnIdx, i) % 512ui32);
                    }
                    else
                    {
                        // should not happen
                        itemType.stackable.Max = 511ui32;
                    }
                }
            }

            strValue = doc.GetCellString(levelColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.level.Quality = doc.GetCellUInt16(levelColumnIdx, i);
            }

            strValue = doc.GetCellString(magiclvlColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.level.Magic = doc.GetCellUInt16(magiclvlColumnIdx, i);
            }

            strValue = doc.GetCellString(reqstrColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.req.Strength = doc.GetCellUInt16(reqstrColumnIdx, i);
            }

            strValue = doc.GetCellString(reqdexColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.req.Dexterity = doc.GetCellUInt16(reqdexColumnIdx, i);
            }

            strValue = doc.GetCellString(levelreqColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.req.Level = doc.GetCellUInt16(levelreqColumnIdx, i);
            }

            strValue = doc.GetCellString(invwidthColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.dimensions.Width = doc.GetCellUInt16(invwidthColumnIdx, i);
            }

            strValue = doc.GetCellString(invheightColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.dimensions.Height = doc.GetCellUInt16(invheightColumnIdx, i);
            }

            itemType.inv_file = doc.GetCellString(invfileColumnIdx, i);

            strValue = doc.GetCellString(invTransColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.inv_transform = doc.GetCellUInt16(invTransColumnIdx, i);
            }

            strValue = doc.GetCellString(gemsocketsColumnIdx, i);
            if (!strValue.empty())
            {
                // has sockets
                std::uint8_t maxSockets = std::uint8_t(doc.GetCellUInt16(gemsocketsColumnIdx, i));
                strValue = doc.GetCellString(gemapplytypeColumnIdx, i);
                if (!strValue.empty())
                {
                    std::uint8_t gemApplyType = std::uint8_t(doc.GetCellUInt16(gemapplytypeColumnIdx, i));
                    switch (gemApplyType)
                    {
                    case 0:
                    case 1:
                    case 2:
                        itemType.gemApplyType = gemApplyType;
                        break;

                    default:
                        // item won't be able to have sockets.
                        maxSockets = 0;
                        break;
                    }
                }

                if (maxSockets > 0)
                {
                    strValue = codes.front();
                    auto& cat = GetItemCategory(strValue);
                    if (cat.code == strValue)
                    {
                        for (const auto& value : cat.max_sockets)
                        {
                            itemType.max_sockets[value.first] = std::min(value.second, maxSockets);
                        }
                    }
                    else
                    {
                        itemType.max_sockets[1] = maxSockets;
                    }
                }
            }

            if (!codes.empty())
            {
                // get equippable locations and quiver type 
                strValue = codes.front();
                auto& cat = GetItemCategory(strValue);
                if (cat.code == strValue)
                {
                    itemType.bodyLocations = cat.bodyLocations;
                    itemType.quiverCode = cat.quiverCode;
                }
            }

            // get categories
            bool bAddType2Category = true;
            bool bAddQuestCategory = false;
            strValue = doc.GetCellString(questColumnIdx, i);
            if (!strValue.empty() && (strValue != "0") && (code != "leg")) // Writ's leg is not marked as a "Quest" item
            {
                bAddQuestCategory = true;
            }

            while (!codes.empty())
            {
                strValue = codes.front();
                codes.pop_front();
                auto& cat = GetItemCategory(strValue);
                if (cat.code == strValue)
                {
                    // make sure the category does not already exist
                    bool bFound = false;
                    for (const auto& val : itemType.categories)
                    {
                        if (val == cat.name)
                        {
                            bFound = true;
                            break;
                        }
                    }

                    if (!bFound)
                    {
                        itemType.categories.push_back(cat.name);
                        codes.insert(codes.end(), cat.subCodes.begin(), cat.subCodes.end());
                        if (cat.beltable)
                        {
                            itemType.beltable = true;
                        }
                    }
                }

                if (codes.empty() && (bAddType2Category || bAddQuestCategory))
                {
                    if (bAddType2Category)
                    {
                        strValue = doc.GetCellString(type2ColumnIdx, i);
                        if (!strValue.empty())
                        {
                            codes.push_back(strValue);
                        }
                        bAddType2Category = false;
                    }

                    if (codes.empty() && bAddQuestCategory)
                    {
                        codes.push_back("ques");
                        bAddQuestCategory = false;
                    }
                }
            }

            strValue = doc.GetCellString(nameableColumnIdx, i);
            if (!strValue.empty() && (strValue != "0"))
            {
                itemType.nameable = true;
            }

            strValue = doc.GetCellString(skipNameColumnIdx, i);
            if (!strValue.empty() && (strValue != "0"))
            {
                itemType.skipName = true;
            }
        }

        s_ItemWeaponType.swap(itemWeaponType);
    }

    std::map<std::string, ItemType> s_ItemArmorType;
    const ItemType& GetArmorItemType(const std::string& code)
    {
        auto iter = s_ItemArmorType.find(code);
        if (iter != s_ItemArmorType.end())
        {
            return iter->second;
        }

        return s_invalidItemType;
    }

    void InitItemArmorTypesData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemArmorType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemArmorType.clear();
        }

        InitItemWeaponTypesData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetArmorTxt());
        auto& doc = *pDoc;
        std::map<std::string, ItemType> itemArmorType;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T nameColumnIdx = doc.GetColumnIdx("name");
        if (nameColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T versionColumnIdx = doc.GetColumnIdx("version");
        if (versionColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T compactsaveColumnIdx = doc.GetColumnIdx("compactsave");
        if (compactsaveColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T minacColumnIdx = doc.GetColumnIdx("minac");
        if (minacColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T maxacColumnIdx = doc.GetColumnIdx("maxac");
        if (maxacColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T durabilityColumnIdx = doc.GetColumnIdx("durability");
        if (durabilityColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T nodurabilityColumnIdx = doc.GetColumnIdx("nodurability");
        if (nodurabilityColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T codeColumnIdx = doc.GetColumnIdx("code");
        if (codeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T namestrColumnIdx = doc.GetColumnIdx("namestr");
        if (namestrColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T typeColumnIdx = doc.GetColumnIdx("type");
        if (typeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T type2ColumnIdx = doc.GetColumnIdx("type2");
        if (type2ColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T stackableColumnIdx = doc.GetColumnIdx("stackable");
        if (stackableColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T minstackColumnIdx = doc.GetColumnIdx("minstack");
        if (minstackColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T maxstackColumnIdx = doc.GetColumnIdx("maxstack");
        if (maxstackColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T levelColumnIdx = doc.GetColumnIdx("level");
        if (levelColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T magiclvlColumnIdx = doc.GetColumnIdx("magic lvl");
        if (magiclvlColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T reqstrColumnIdx = doc.GetColumnIdx("reqstr");
        if (reqstrColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T levelreqColumnIdx = doc.GetColumnIdx("levelreq");
        if (levelreqColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invwidthColumnIdx = doc.GetColumnIdx("invwidth");
        if (invwidthColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invheightColumnIdx = doc.GetColumnIdx("invheight");
        if (invheightColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invfileColumnIdx = doc.GetColumnIdx("invfile");
        if (invfileColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invTransColumnIdx = doc.GetColumnIdx("InvTrans");
        if (invTransColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T questColumnIdx = doc.GetColumnIdx("quest");
        if (questColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T gemsocketsColumnIdx = doc.GetColumnIdx("gemsockets");
        if (gemsocketsColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T gemapplytypeColumnIdx = doc.GetColumnIdx("gemapplytype");
        if (gemapplytypeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T beltColumnIdx = doc.GetColumnIdx("belt");
        if (beltColumnIdx < 0)
        {
            return;
        }

        SSIZE_T nameableColumnIdx = doc.GetColumnIdx("Nameable");
        if (nameableColumnIdx < 0)
        {
            nameableColumnIdx = doc.GetColumnIdx("nameable"); // support the alternate version of this file format
            if (nameableColumnIdx < 0)
            {
                return;
            }
        }

        const SSIZE_T skipNameColumnIdx = doc.GetColumnIdx("SkipName");
        if (skipNameColumnIdx < 0)
        {
            return;
        }

        std::string strValue;
        std::string code;
        std::string name;
        std::deque<std::string> codes;
        for (size_t i = 0; i < numRows; ++i)
        {
            codes.clear();
            code = doc.GetCellString(codeColumnIdx, i);
            if (code.empty())
            {
                // skip
                continue;
            }

            auto& itemType = itemArmorType[code];
            itemType.code = code;
            itemType.name = doc.GetCellString(nameColumnIdx, i);
            strValue = doc.GetCellString(namestrColumnIdx, i);
            if (strValue.empty())
            {
                if(LocalizationHelpers::GetStringTxtValue(code, name))
                {
                    itemType.name = name;
                }
            }
            else if (LocalizationHelpers::GetStringTxtValue(strValue, name))
            {
                itemType.name = name;
            }

            strValue = doc.GetCellString(versionColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.version = doc.GetCellUInt16(versionColumnIdx, i);
                if (itemType.version == 0)
                {
                    itemType.code_v100 = s_ItemCurTypeCodeIdx_v100;
                    ++s_ItemCurTypeCodeIdx_v100;
                    s_ItemTypeCodes_v100[itemType.code_v100] = itemType.code;
                }
            }

            strValue = doc.GetCellString(compactsaveColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.compactsave = doc.GetCellUInt16(compactsaveColumnIdx, i) != 0 ? true : false;
            }

            strValue = doc.GetCellString(minacColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.ac.Min = doc.GetCellUInt16(minacColumnIdx, i);
            }

            strValue = doc.GetCellString(maxacColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.ac.Max = doc.GetCellUInt16(maxacColumnIdx, i);
            }

            strValue = doc.GetCellString(durabilityColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.durability.Base = doc.GetCellUInt16(durabilityColumnIdx, i);
                if (itemType.durability.Base != 0)
                {
                    strValue = doc.GetCellString(nodurabilityColumnIdx, i);
                    if (strValue.empty() || (doc.GetCellUInt16(nodurabilityColumnIdx, i) == 0))
                    {
                        itemType.durability.Max = itemType.durability.Base;
                    }
                }
            }

            strValue = doc.GetCellString(typeColumnIdx, i);
            if (!strValue.empty())
            {
                codes.push_back(strValue);
            }

            strValue = doc.GetCellString(stackableColumnIdx, i);
            if (!strValue.empty())
            {
                if (doc.GetCellUInt16(stackableColumnIdx, i) != 0)
                {
                    strValue = doc.GetCellString(minstackColumnIdx, i);
                    if (!strValue.empty())
                    {
                        itemType.stackable.Min = doc.GetCellUInt32(minstackColumnIdx, i) % 512ui32;
                    }

                    strValue = doc.GetCellString(maxstackColumnIdx, i);
                    if (!strValue.empty())
                    {
                        itemType.stackable.Max = std::max(itemType.stackable.Min, doc.GetCellUInt32(maxstackColumnIdx, i) % 512ui32);
                    }
                    else
                    {
                        // should not happen
                        itemType.stackable.Max = 511ui32;
                    }
                }
            }

            strValue = doc.GetCellString(levelColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.level.Quality = doc.GetCellUInt16(levelColumnIdx, i);
            }

            strValue = doc.GetCellString(magiclvlColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.level.Magic = doc.GetCellUInt16(magiclvlColumnIdx, i);
            }

            strValue = doc.GetCellString(reqstrColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.req.Strength = doc.GetCellUInt16(reqstrColumnIdx, i);
            }

            strValue = doc.GetCellString(levelreqColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.req.Level = doc.GetCellUInt16(levelreqColumnIdx, i);
            }

            strValue = doc.GetCellString(invwidthColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.dimensions.Width = doc.GetCellUInt16(invwidthColumnIdx, i);
            }

            strValue = doc.GetCellString(invheightColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.dimensions.Height = doc.GetCellUInt16(invheightColumnIdx, i);
            }

            itemType.inv_file = doc.GetCellString(invfileColumnIdx, i);

            strValue = doc.GetCellString(invTransColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.inv_transform = doc.GetCellUInt16(invTransColumnIdx, i);
            }

            strValue = doc.GetCellString(gemsocketsColumnIdx, i);
            if (!strValue.empty())
            {
                // has sockets
                std::uint8_t maxSockets = std::uint8_t(doc.GetCellUInt16(gemsocketsColumnIdx, i));
                strValue = doc.GetCellString(gemapplytypeColumnIdx, i);
                if (!strValue.empty())
                {
                    std::uint8_t gemApplyType = std::uint8_t(doc.GetCellUInt16(gemapplytypeColumnIdx, i));
                    switch (gemApplyType)
                    {
                    case 0:
                    case 1:
                    case 2:
                        itemType.gemApplyType = gemApplyType;
                        break;

                    default:
                        // item won't be able to have sockets.
                        maxSockets = 0;
                        break;
                    }
                }

                if (maxSockets > 0)
                {
                    strValue = codes.front();
                    auto& cat = GetItemCategory(strValue);
                    if (cat.code == strValue)
                    {
                        for (const auto& value : cat.max_sockets)
                        {
                            itemType.max_sockets[value.first] = std::min(value.second, maxSockets);
                        }
                    }
                    else
                    {
                        itemType.max_sockets[1] = maxSockets;
                    }
                }
            }

            // get equippable locations
            if (!codes.empty())
            {
                strValue = codes.front();
                auto& cat = GetItemCategory(strValue);
                if (cat.code == strValue)
                {
                    itemType.bodyLocations = cat.bodyLocations;
                }
            }

            // get categories
            bool bAddType2Category = true;
            bool bAddQuestCategory = false;
            strValue = doc.GetCellString(questColumnIdx, i);
            if (!strValue.empty() && (strValue != "0"))
            {
                bAddQuestCategory = true;
            }

            while (!codes.empty())
            {
                strValue = codes.front();
                codes.pop_front();
                auto& cat = GetItemCategory(strValue);
                if (cat.code == strValue)
                {
                    // make sure the category does not already exist
                    bool bFound = false;
                    for (const auto& val : itemType.categories)
                    {
                        if (val == cat.name)
                        {
                            bFound = true;
                            break;
                        }
                    }

                    if (!bFound)
                    {
                        itemType.categories.push_back(cat.name);
                        codes.insert(codes.end(), cat.subCodes.begin(), cat.subCodes.end());
                        if (cat.beltable)
                        {
                            itemType.beltable = true;
                        }
                    }
                }

                if (codes.empty() && (bAddType2Category || bAddQuestCategory))
                {
                    if (bAddType2Category)
                    {
                        strValue = doc.GetCellString(type2ColumnIdx, i);
                        if (!strValue.empty())
                        {
                            codes.push_back(strValue);
                        }
                        bAddType2Category = false;
                    }

                    if (codes.empty() && bAddQuestCategory)
                    {
                        codes.push_back("ques");
                        bAddQuestCategory = false;
                    }
                }
            }

            if (itemType.isBelt())
            {
                itemType.dimensions.InvWidth = 4ui16; // all belts are 4 slots wide
                std::uint16_t numSlots(12ui16);
                if (!s_ItemBeltSlots.empty())
                {
                    std::uint16_t beltIdx = 0;
                    strValue = doc.GetCellString(beltColumnIdx, i);
                    if (!strValue.empty())
                    {
                        beltIdx = doc.GetCellUInt16(beltColumnIdx, i);
                        if (beltIdx >= s_ItemBeltSlots.size())
                        {
                            beltIdx = 0;
                        }
                    }

                    numSlots = s_ItemBeltSlots[beltIdx];
                }

                itemType.dimensions.InvHeight = numSlots / itemType.dimensions.InvWidth;
            }
            
            strValue = doc.GetCellString(nameableColumnIdx, i);
            if (!strValue.empty() && (strValue != "0"))
            {
                itemType.nameable = true;
            }

            strValue = doc.GetCellString(skipNameColumnIdx, i);
            if (!strValue.empty() && (strValue != "0"))
            {
                itemType.skipName = true;
            }
        }

        s_ItemArmorType.swap(itemArmorType);
    }

    std::map<std::string, ItemType> s_ItemMiscType;
    const ItemType& GetMiscItemType(std::string code)
    {
        auto iter = s_ItemMiscType.find(code);
        if (iter != s_ItemMiscType.end())
        {
            return iter->second;
        }

        return s_invalidItemType;
    }

    void InitItemMiscTypesData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemMiscType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemCurTypeCodeIdx_v100 = 0;
            s_ItemTypeCodes_v100.clear();
            s_ItemArmorType.clear();
            s_ItemWeaponType.clear();
            s_ItemMiscType.clear();
        }

        InitItemArmorTypesData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetMiscTxt());
        auto& doc = *pDoc;
        std::map<std::string, ItemType> itemMiscType;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T nameColumnIdx = doc.GetColumnIdx("name");
        if (nameColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T name2ColumnIdx = doc.GetColumnIdx("*name"); // support the alternate version of this file format

        const SSIZE_T versionColumnIdx = doc.GetColumnIdx("version");
        if (versionColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T compactsaveColumnIdx = doc.GetColumnIdx("compactsave");
        if (compactsaveColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T codeColumnIdx = doc.GetColumnIdx("code");
        if (codeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T namestrColumnIdx = doc.GetColumnIdx("namestr");
        if (namestrColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T typeColumnIdx = doc.GetColumnIdx("type");
        if (typeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T type2ColumnIdx = doc.GetColumnIdx("type2");
        if (type2ColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T stackableColumnIdx = doc.GetColumnIdx("stackable");
        if (stackableColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T minstackColumnIdx = doc.GetColumnIdx("minstack");
        if (minstackColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T maxstackColumnIdx = doc.GetColumnIdx("maxstack");
        if (maxstackColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T levelColumnIdx = doc.GetColumnIdx("level");
        if (levelColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T levelreqColumnIdx = doc.GetColumnIdx("levelreq");
        if (levelreqColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invwidthColumnIdx = doc.GetColumnIdx("invwidth");
        if (invwidthColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invheightColumnIdx = doc.GetColumnIdx("invheight");
        if (invheightColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invfileColumnIdx = doc.GetColumnIdx("invfile");
        if (invfileColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invTransColumnIdx = doc.GetColumnIdx("InvTrans");
        if (invTransColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T questColumnIdx = doc.GetColumnIdx("quest");
        if (questColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T gemsocketsColumnIdx = doc.GetColumnIdx("gemsockets");
        if (gemsocketsColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T gemapplytypeColumnIdx = doc.GetColumnIdx("gemapplytype");
        if (gemapplytypeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T nameableColumnIdx = doc.GetColumnIdx("Nameable");
        if (nameableColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T skipNameColumnIdx = doc.GetColumnIdx("SkipName");
        if (skipNameColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T stat1ColumnIdx = doc.GetColumnIdx("stat1");
        if (stat1ColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T spelldescColumnIdx = doc.GetColumnIdx("spelldesc");
        if (spelldescColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T spelldescstrColumnIdx = doc.GetColumnIdx("spelldescstr");
        if (spelldescstrColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T spelldesccalcColumnIdx = doc.GetColumnIdx("spelldesccalc");
        if (spelldesccalcColumnIdx < 0)
        {
            return;
        }

        std::string strValue;
        std::string name;
        std::string code;
        std::deque<std::string> codes;
        for (size_t i = 0; i < numRows; ++i)
        {
            codes.clear();
            code = doc.GetCellString(codeColumnIdx, i);
            if (code.empty())
            {
                // skip
                continue;
            }

            auto& itemType = itemMiscType[code];
            itemType.code = code;

            bool bAddUnused = false;
            itemType.name = doc.GetCellString(nameColumnIdx, i);
            if (name2ColumnIdx >= 0)
            {
                if (itemType.name.compare("Not used") == 0)
                {
                    bAddUnused = true;
                    itemType.name = doc.GetCellString(name2ColumnIdx, i);
                }
            }
            else
            {
                // mark unused items
                static std::set<std::string> unusedItems = { "Heart", "Brain", "Jawbone", "Eye", "Horn",
                    "Tail", "Flag", "Fang", "Quill", "Soul", "Scalp", "Spleen" };
                if (unusedItems.find(itemType.name) != unusedItems.end())
                {
                    bAddUnused = true;
                }
            }

            strValue = doc.GetCellString(namestrColumnIdx, i);
            if (strValue.empty())
            {
                if (LocalizationHelpers::GetStringTxtValue(code, name))
                {
                    itemType.name = name;
                }
            }
            else if (LocalizationHelpers::GetStringTxtValue(strValue, name))
            {
                itemType.name = name;
            }

            strValue = doc.GetCellString(versionColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.version = doc.GetCellUInt16(versionColumnIdx, i);
                if (itemType.version == 0)
                {
                    itemType.code_v100 = s_ItemCurTypeCodeIdx_v100;
                    ++s_ItemCurTypeCodeIdx_v100;
                    s_ItemTypeCodes_v100[itemType.code_v100] = itemType.code;
                }
            }

            strValue = doc.GetCellString(compactsaveColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.compactsave = doc.GetCellUInt16(compactsaveColumnIdx, i) != 0 ? true : false;
            }

            strValue = doc.GetCellString(typeColumnIdx, i);
            if (!strValue.empty())
            {
                codes.push_back(strValue);
            }

            strValue = doc.GetCellString(stackableColumnIdx, i);
            if (!strValue.empty())
            {
                if (doc.GetCellUInt16(stackableColumnIdx, i) != 0)
                {
                    strValue = doc.GetCellString(minstackColumnIdx, i);
                    if (!strValue.empty())
                    {
                        itemType.stackable.Min = doc.GetCellUInt32(minstackColumnIdx, i) % 512ui32;
                    }

                    strValue = doc.GetCellString(maxstackColumnIdx, i);
                    if (!strValue.empty())
                    {
                        itemType.stackable.Max = std::max(itemType.stackable.Min, doc.GetCellUInt32(maxstackColumnIdx, i) % 512ui32);
                    }
                    else
                    {
                        // should not happen
                        itemType.stackable.Max = 511ui32;
                    }
                }
            }

            strValue = doc.GetCellString(levelColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.level.Quality = doc.GetCellUInt16(levelColumnIdx, i);
            }

            strValue = doc.GetCellString(levelreqColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.req.Level = doc.GetCellUInt16(levelreqColumnIdx, i);
            }

            strValue = doc.GetCellString(invwidthColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.dimensions.Width = doc.GetCellUInt16(invwidthColumnIdx, i);
            }

            strValue = doc.GetCellString(invheightColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.dimensions.Height = doc.GetCellUInt16(invheightColumnIdx, i);
            }

            itemType.inv_file = doc.GetCellString(invfileColumnIdx, i);

            strValue = doc.GetCellString(invTransColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.inv_transform = doc.GetCellUInt16(invTransColumnIdx, i);
            }

            strValue = doc.GetCellString(gemsocketsColumnIdx, i);
            if (!strValue.empty())
            {
                // has sockets
                std::uint8_t maxSockets = std::uint8_t(doc.GetCellUInt16(gemsocketsColumnIdx, i));
                strValue = doc.GetCellString(gemapplytypeColumnIdx, i);
                if (!strValue.empty())
                {
                    std::uint8_t gemApplyType = std::uint8_t(doc.GetCellUInt16(gemapplytypeColumnIdx, i));
                    switch (gemApplyType)
                    {
                    case 0:
                    case 1:
                    case 2:
                        itemType.gemApplyType = gemApplyType;
                        break;

                    default:
                        // item won't be able to have sockets.
                        maxSockets = 0;
                        break;
                    }
                }

                if (maxSockets > 0)
                {
                    strValue = codes.front();
                    auto& cat = GetItemCategory(strValue);
                    if (cat.code == strValue)
                    {
                        for (const auto& value : cat.max_sockets)
                        {
                            itemType.max_sockets[value.first] = std::min(value.second, maxSockets);
                        }
                    }
                    else
                    {
                        itemType.max_sockets[1] = maxSockets;
                    }
                }
            }

            // get equippable locations
            if (!codes.empty())
            {
                strValue = codes.front();
                auto& cat = GetItemCategory(strValue);
                if (cat.code == strValue)
                {
                    itemType.bodyLocations = cat.bodyLocations;
                }
            }

            // get categories
            bool bAddType2Category = true;
            bool bAddQuestCategory = false;
            strValue = doc.GetCellString(questColumnIdx, i);
            if (!strValue.empty() && (strValue != "0"))
            {
                bAddQuestCategory = true;
            }

            while (!codes.empty())
            {
                strValue = codes.front();
                codes.pop_front();
                auto& cat = GetItemCategory(strValue);
                if (cat.code == strValue)
                {
                    // make sure the category does not already exist
                    bool bFound = false;
                    for (const auto& val : itemType.categories)
                    {
                        if (val == cat.name)
                        {
                            bFound = true;
                            break;
                        }
                    }

                    if (!bFound)
                    {
                        itemType.categories.push_back(cat.name);
                        codes.insert(codes.end(), cat.subCodes.begin(), cat.subCodes.end());
                        if (cat.beltable)
                        {
                            itemType.beltable = true;
                        }
                    }
                }

                if (codes.empty() && (bAddType2Category || bAddQuestCategory))
                {
                    if (bAddType2Category)
                    {
                        strValue = doc.GetCellString(type2ColumnIdx, i);
                        if (!strValue.empty())
                        {
                            codes.push_back(strValue);
                        }
                        bAddType2Category = false;
                    }

                    if (codes.empty() && bAddQuestCategory)
                    {
                        codes.push_back("ques");
                        bAddQuestCategory = false;
                    }
                }
            }

            strValue = doc.GetCellString(nameableColumnIdx, i);
            if (!strValue.empty() && (strValue != "0"))
            {
                itemType.nameable = true;
            }

            strValue = doc.GetCellString(skipNameColumnIdx, i);
            if (!strValue.empty() && (strValue != "0"))
            {
                itemType.skipName = true;
            }

            if (itemType.isHoradricCube())
            {
                itemType.dimensions.InvWidth = 3ui16;
                itemType.dimensions.InvHeight = 4ui16;
            }
            else if (itemType.isCharm())
            {
                // fix up graphic usage
                std::stringstream ss;
                ss << "invchm";
                ss << code.back();
                itemType.inv_file = ss.str();
            }
            else if (itemType.isJewel())
            {
                // fix up graphic usage
                std::stringstream ss;
                ss << "inv";
                ss << code;
                itemType.inv_file = ss.str();
            }

            if (!bAddUnused && itemType.isPotion())
            {
                strValue = doc.GetCellString(stat1ColumnIdx, i);
                bAddUnused = strValue.empty();
                if (bAddUnused)
                {
                    strValue = "unused";
                    LocalizationHelpers::GetStringTxtValue(strValue, name, "an evil force");
                    itemType.name = name;
                }
                else
                {
                    strValue = doc.GetCellString(spelldescColumnIdx, i);
                    if (!strValue.empty() && (strValue != "0"))
                    {
                        itemType.spellDesc.descFunc = std::uint8_t(doc.GetCellUInt16(spelldescColumnIdx, i));
                        strValue = doc.GetCellString(spelldescstrColumnIdx, i);
                        if (!strValue.empty())
                        {
                            LocalizationHelpers::GetStringTxtValue(strValue, itemType.spellDesc.desc);
                        }

                        strValue = doc.GetCellString(spelldesccalcColumnIdx, i);
                        if (!strValue.empty())
                        {
                            itemType.spellDesc.descCalc = doc.GetCellUInt64(spelldesccalcColumnIdx, i);
                        }
                    }
                }
            }

            if (bAddUnused)
            {
                itemType.categories.push_back("Unused");
            }
        }

        s_ItemMiscType.swap(itemMiscType);
    }

    const d2ce::ItemType& GetItemTypeHelper(const std::string& code)
    {
        // Could be armor
        {
            const auto& result = GetArmorItemType(code);
            if (&result != &s_invalidItemType)
            {
                return result;
            }
        }

        // Could be a weapon
        {
            const auto& result = GetWeaponItemType(code);
            if (&result != &s_invalidItemType)
            {
                return result;
            }
        }

        // Could be a Misc.
        return GetMiscItemType(code);
    }

    const d2ce::ItemType& GetItemTypeHelper(const std::array<std::uint8_t, 4>& strcode)
    {
        std::string testStr("   ");
        testStr[0] = (char)strcode[0];
        testStr[1] = (char)strcode[1];
        testStr[2] = (char)strcode[2];
        return GetItemTypeHelper(testStr);
    }

    struct ItemAffixLevelType
    {
        // The quality level (qLvl) of this affix, for this affix to be available on an item, that item must have an item level (iLvl) of at least this amount.
        std::uint16_t level = 0;

        // An extremely powerful but badly underused field. This controls the point at which this affix will no longer appear on items
        std::uint16_t maxLevel = 0;
        
        // The general level requirement that your character must meet before he can use an item that has this affix.
        std::uint16_t levelreq = 0;

        void clear()
        {
            level = 0;
            maxLevel = 0;
            levelreq = 0;
        }
    };

    struct ItemAffixClassType
    {
        std::string specific;
        std::string name;   // The character class for whom the class specific level requirement appears.
        std::uint16_t level = 0; // The level requirement your character must meet before he can use an item with this affix if his class is the class specified in the Class column
    };

    struct ItemAffixModType
    {
        std::string code;  // The modifier(s) granted by this affix. This is an ID pointer from PROPERTIES.txt.
        std::string param; // The parameter passed to the associated modifier
        std::string min;   // The minimum value passed to the associated modifier
        std::string max;   // The maximum value passed to the associated modifier
    };

    struct ItemAffixType
    {
        std::uint16_t code = 0; // index of the affix

        std::string index; // The ID pointer that is referenced by the game to get the localized name
        std::string name; // what string will be displayed in-game for this affix

        //   0 = pre v1.08 affixes
        //   1 - Non-Expansion (post v1.08 affixes) (affixes available in classic and LoD).
        // 100 - Expansion only affixes
        std::uint16_t version = 0;

        ItemAffixLevelType level;
        ItemAffixClassType classType;

        std::uint16_t group = 0; // he group an affix is assigned to. The game cannot pick more then one affix from each group

        std::vector<ItemAffixModType> modType;

        // An ID pointer from COLORS.txt, this determines what color the modifier will give this item, or empty string if no transform color is applied.
        std::string transform_color;

        std::vector<std::string> included_categories; // what item types this affix can appear on
        std::vector<std::string> excluded_categories; // what item types this affix will never appear on
    };

    void InitItemMagicAffixData(ITxtDocument& doc, std::map<std::uint16_t, ItemAffixType>& sItemMagicAffixType)
    {
        std::map<std::uint16_t, ItemAffixType> itemMagicAffixType;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T nameColumnIdx = doc.GetColumnIdx("Name");
        if (nameColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T versionColumnIdx = doc.GetColumnIdx("version");
        if (versionColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T levelColumnIdx = doc.GetColumnIdx("level");
        if (levelColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T maxlevelColumnIdx = doc.GetColumnIdx("maxlevel");
        if (maxlevelColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T levelreqColumnIdx = doc.GetColumnIdx("levelreq");
        if (levelreqColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T classspecificColumnIdx = doc.GetColumnIdx("classspecific");
        if (classspecificColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T classColumnIdx = doc.GetColumnIdx("class");
        if (classColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T classlevelreqColumnIdx = doc.GetColumnIdx("classlevelreq");
        if (classlevelreqColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T groupColumnIdx = doc.GetColumnIdx("group");
        if (groupColumnIdx < 0)
        {
            return;
        }

        size_t modParamSize = 3;
        std::vector<SSIZE_T> modCode(modParamSize, -1);
        std::vector<SSIZE_T> modParam(modParamSize, -1);
        std::vector<SSIZE_T> modMin(modParamSize, -1);
        std::vector<SSIZE_T> modMax(modParamSize, -1);
        for (size_t idx = 1; idx <= modParamSize; ++idx)
        {
            modCode[idx - 1] = doc.GetColumnIdx("mod" + std::to_string(idx) + "code");
            if (modCode[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }

            modParam[idx - 1] = doc.GetColumnIdx("mod" + std::to_string(idx) + "param");
            if (modParam[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }

            modMin[idx - 1] = doc.GetColumnIdx("mod" + std::to_string(idx) + "min");
            if (modMin[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }

            modMax[idx - 1] = doc.GetColumnIdx("mod" + std::to_string(idx) + "max");
            if (modMax[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }
        }

        const SSIZE_T transformColumnIdx = doc.GetColumnIdx("transform");
        const SSIZE_T transformcolorColumnIdx = doc.GetColumnIdx("transformcolor");
        if (transformcolorColumnIdx < 0)
        {
            return;
        }

        size_t itypesParamSize = 7;
        std::vector<SSIZE_T> itypes(itypesParamSize, -1);
        for (size_t idx = 1; idx <= itypesParamSize; ++idx)
        {
            itypes[idx - 1] = doc.GetColumnIdx("itype" + std::to_string(idx));
            if (itypes[idx - 1] < 0)
            {
                itypesParamSize = idx - 1;
                itypes.resize(itypesParamSize);
                break;
            }
        }

        size_t etypesParamSize = 5;
        std::vector<SSIZE_T> etypes(etypesParamSize, -1);
        for (size_t idx = 1; idx <= etypesParamSize; ++idx)
        {
            etypes[idx - 1] = doc.GetColumnIdx("etype" + std::to_string(idx));
            if (etypes[idx - 1] < 0)
            {
                etypesParamSize = idx - 1;
                etypes.resize(etypesParamSize);
                break;
            }
        }

        std::string strValue;
        std::uint16_t code = 0;
        for (size_t i = 0; i < numRows; ++i)
        {
            strValue = doc.GetCellString(nameColumnIdx, i);
            if (strValue.empty() || (strValue == "Expansion"))
            {
                // skip
                continue;
            }

            code = std::uint16_t(i);
            auto& itemType = itemMagicAffixType[code];
            itemType.code = code;
            itemType.index = strValue;
            LocalizationHelpers::GetStringTxtValue(itemType.index, itemType.name);

            strValue = doc.GetCellString(versionColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.version = doc.GetCellUInt16(versionColumnIdx, i);
            }

            strValue = doc.GetCellString(levelColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.level.level = doc.GetCellUInt16(levelColumnIdx, i);
            }

            strValue = doc.GetCellString(maxlevelColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.level.maxLevel = doc.GetCellUInt16(maxlevelColumnIdx, i);
            }

            strValue = doc.GetCellString(levelreqColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.level.levelreq = doc.GetCellUInt16(levelreqColumnIdx, i);
            }

            itemType.classType.specific = doc.GetCellString(classspecificColumnIdx, i);
            itemType.classType.name = doc.GetCellString(classColumnIdx, i);

            strValue = doc.GetCellString(classlevelreqColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.classType.level = doc.GetCellUInt16(classlevelreqColumnIdx, i);
            }

            strValue = doc.GetCellString(groupColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.group = doc.GetCellUInt16(groupColumnIdx, i);
            }

            for (size_t idx = 0; idx < modParamSize; ++idx)
            {
                strValue = doc.GetCellString(modCode[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                itemType.modType.resize(itemType.modType.size() + 1);
                auto& mod = itemType.modType.back();
                mod.code = strValue;

                strValue = doc.GetCellString(modParam[idx], i);
                if (!strValue.empty())
                {
                    mod.param = strValue;
                }

                strValue = doc.GetCellString(modMin[idx], i);
                if (!strValue.empty())
                {
                    mod.min = strValue;
                }

                strValue = doc.GetCellString(modMax[idx], i);
                if (!strValue.empty())
                {
                    mod.max = strValue;
                }
            }

            if (transformColumnIdx >= 0)
            {
                // support the alternate version of this file format
                strValue = doc.GetCellString(transformColumnIdx, i);
                if (!strValue.empty() && (strValue != "0"))
                {
                    itemType.transform_color = doc.GetCellString(transformcolorColumnIdx, i);
                }
            }
            else
            {
                itemType.transform_color = doc.GetCellString(transformcolorColumnIdx, i);
            }

            for (size_t idx = 0; idx < itypesParamSize; ++idx)
            {
                strValue = doc.GetCellString(itypes[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                auto& cat = GetItemCategory(strValue);
                if (cat.code != strValue)
                {
                    break;
                }

                itemType.included_categories.push_back(cat.name);
            }

            for (size_t idx = 0; idx < etypesParamSize; ++idx)
            {
                strValue = doc.GetCellString(etypes[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                auto& cat = GetItemCategory(strValue);
                if (cat.code != strValue)
                {
                    break;
                }

                itemType.excluded_categories.push_back(cat.name);
            }
        }

        sItemMagicAffixType.swap(itemMagicAffixType);
    }

    std::map<std::uint16_t, ItemAffixType> s_ItemMagicPrefixType;
    void InitItemMagicPrefixData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr; 
        if (!s_ItemMagicPrefixType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemMagicPrefixType.clear();
        }

        InitItemPropertiesData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetMagicPrefixTxt());
        auto& doc = *pDoc;
        InitItemMagicAffixData(doc, s_ItemMagicPrefixType);
    }

    std::map<std::uint16_t, ItemAffixType> s_ItemMagicSuffixType;
    void InitItemMagicSuffixData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemMagicSuffixType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemMagicSuffixType.clear();
        }

        InitItemMagicPrefixData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetMagicSuffixTxt());
        auto& doc = *pDoc;
        InitItemMagicAffixData(doc, s_ItemMagicSuffixType);
    }

    std::map<std::string, std::uint16_t> s_ItemRareIndex;
    std::map<std::string, std::uint16_t> s_ItemRareNames;
    std::uint16_t InitItemRareAffixData(ITxtDocument& doc, std::map<std::uint16_t, ItemAffixType>& sItemMagicAffixType, std::uint16_t offset = 0)
    {
        std::map<std::uint16_t, ItemAffixType> itemMagicAffixType;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T nameColumnIdx = doc.GetColumnIdx("name");
        if (nameColumnIdx < 0)
        {
            return 0;
        }

        const SSIZE_T versionColumnIdx = doc.GetColumnIdx("version");
        if (versionColumnIdx < 0)
        {
            return 0;
        }

        size_t itypesParamSize = 7;
        std::vector<SSIZE_T> itypes(itypesParamSize, -1);
        for (size_t idx = 1; idx <= itypesParamSize; ++idx)
        {
            itypes[idx - 1] = doc.GetColumnIdx("itype" + std::to_string(idx));
            if (itypes[idx - 1] < 0)
            {
                itypesParamSize = idx - 1;
                itypes.resize(itypesParamSize);
                break;
            }
        }

        size_t etypesParamSize = 4;
        std::vector<SSIZE_T> etypes(etypesParamSize, -1);
        for (size_t idx = 1; idx <= etypesParamSize; ++idx)
        {
            etypes[idx - 1] = doc.GetColumnIdx("etype" + std::to_string(idx));
            if (etypes[idx - 1] < 0)
            {
                etypesParamSize = idx - 1;
                etypes.resize(etypesParamSize);
                break;
            }
        }

        std::string strValue;
        std::uint16_t code = 0;
        for (size_t i = 0; i < numRows; ++i)
        {
            strValue = doc.GetCellString(nameColumnIdx, i);
            if (strValue.empty())
            {
                // skip
                continue;
            }

            code = std::uint16_t(i + offset + 1);
            auto& itemType = itemMagicAffixType[code];
            itemType.code = code;
            itemType.index = strValue;
            LocalizationHelpers::GetStringTxtValue(itemType.index, itemType.name);

            s_ItemRareIndex[itemType.index] = code;
            s_ItemRareNames[itemType.name] = code;
            strValue = doc.GetCellString(versionColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.version = doc.GetCellUInt16(versionColumnIdx, i);
            }

            for (size_t idx = 0; idx < itypesParamSize; ++idx)
            {
                strValue = doc.GetCellString(itypes[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                auto& cat = GetItemCategory(strValue);
                if (cat.code != strValue)
                {
                    break;
                }

                itemType.included_categories.push_back(cat.name);
            }

            for (size_t idx = 0; idx < etypesParamSize; ++idx)
            {
                strValue = doc.GetCellString(etypes[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                auto& cat = GetItemCategory(strValue);
                if (cat.code != strValue)
                {
                    break;
                }

                itemType.excluded_categories.push_back(cat.name);
            }
        }

        sItemMagicAffixType.swap(itemMagicAffixType);
        return code;
    }

    std::map<std::uint16_t, ItemAffixType> s_ItemRareSuffixType;
    std::uint16_t InitItemRareSuffixData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemRareSuffixType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return 0;
            }

            s_ItemRareSuffixType.clear();
        }

        InitItemMagicSuffixData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetRareSuffixTxt());
        auto& doc = *pDoc;
        return InitItemRareAffixData(doc, s_ItemRareSuffixType);
    }

    std::map<std::uint16_t, ItemAffixType> s_ItemRarePrefixType;
    void InitItemRarePrefixData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemRarePrefixType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemRareIndex.clear();
            s_ItemRareNames.clear();
            s_ItemRareSuffixType.clear();
            s_ItemRarePrefixType.clear();
        }

        InitItemTypesData(txtReader);
        auto offset = InitItemRareSuffixData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetRarePrefixTxt());
        auto& doc = *pDoc;
        InitItemRareAffixData(doc, s_ItemRarePrefixType, offset);
    }

    const ItemAffixType& getRareAffixType(std::uint16_t id)
    {
        auto iterSuffix = s_ItemRareSuffixType.find(id);
        if (iterSuffix == s_ItemRareSuffixType.end())
        {
            auto iterPrefix = s_ItemRarePrefixType.find(id);
            if (iterPrefix == s_ItemRarePrefixType.end())
            {
                static ItemAffixType invalidValue;
                return invalidValue;
            }

            return iterPrefix->second;
        }

        return iterSuffix->second;
    }

    struct ItemUniqueType
    {
        std::uint16_t id = 0; // index of the unique item

        std::string index; // The ID pointer that is referenced by the game in TreasureClassEx.txt and CubeMain.txt, this also controls the string that will be used to display the item's name in-game.
        std::string name; // what string will be displayed in-game for this unique item

        //   0 = pre v1.08 affixes
        //   1 - Non-Expansion (post v1.08 affixes) (affixes available in classic and LoD).
        // 100 - Expansion only affixes
        std::uint16_t version = 0;

        ItemAffixLevelType level; // The quality level of this unique item and The character level required to use this unique item
        std::string code; // The code of the base form of this unique item, this is an ID pointer from Weapons.txt, Armor.txt or Misc.txt.

        std::string transform_color; // Palette shift to apply to the the DC6 inventory-file
        std::string inv_file; //  Overrides the invfile and uniqueinvfile specified in Weapons.txt, Armor.txt or Misc.txt for the base item.

        std::vector<ItemAffixModType> modType; // Different modifiers a unique item can grant you
    };

    std::map<std::string, std::uint16_t> s_ItemUniqueQuestItemsIndex;
    std::map<std::string, std::uint16_t> s_ItemUniqueItemsIndex;
    std::map<std::uint16_t, ItemUniqueType> s_ItemUniqueItemsType;
    void InitItemUniqueItemsData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemUniqueItemsType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemUniqueQuestItemsIndex.clear();
            s_ItemUniqueItemsIndex.clear();
            s_ItemUniqueItemsType.clear();
        }

        InitItemMiscTypesData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetUniqueItemsTxt());
        auto& doc = *pDoc;
        std::map<std::string, std::uint16_t> itemUniqueQuestItemsIndex;
        std::map<std::string, std::uint16_t> itemUniqueItemsIndex;
        std::map<std::uint16_t, ItemUniqueType> itemUniqueItemsType;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T indexColumnIdx = doc.GetColumnIdx("index");
        if (indexColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T idColumnIdx = doc.GetColumnIdx("*ID");
        const SSIZE_T versionColumnIdx = doc.GetColumnIdx("version");
        if (versionColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T lvlColumnIdx = doc.GetColumnIdx("lvl");
        if (lvlColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T lvlreqColumnIdx = doc.GetColumnIdx("lvl req");
        if (lvlreqColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T codeColumnIdx = doc.GetColumnIdx("code");
        if (codeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invtransformColumnIdx = doc.GetColumnIdx("invtransform");
        if (invtransformColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invfileColumnIdx = doc.GetColumnIdx("invfile");
        if (invfileColumnIdx < 0)
        {
            return;
        }

        size_t modParamSize = 12;
        std::vector<SSIZE_T> modCode(modParamSize, -1);
        std::vector<SSIZE_T> modParam(modParamSize, -1);
        std::vector<SSIZE_T> modMin(modParamSize, -1);
        std::vector<SSIZE_T> modMax(modParamSize, -1);
        for (size_t idx = 1; idx <= modParamSize; ++idx)
        {
            modCode[idx - 1] = doc.GetColumnIdx("prop" + std::to_string(idx));
            if (modCode[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }

            modParam[idx - 1] = doc.GetColumnIdx("par" + std::to_string(idx));
            if (modParam[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }

            modMin[idx - 1] = doc.GetColumnIdx("min" + std::to_string(idx));
            if (modMin[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }

            modMax[idx - 1] = doc.GetColumnIdx("max" + std::to_string(idx));
            if (modMax[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }
        }

        std::string strValue;
        std::string index;
        std::uint16_t id = 0;
        for (size_t i = 0; i < numRows; ++i)
        {
            index = doc.GetCellString(indexColumnIdx, i);
            if (index.empty())
            {
                // skip
                continue;
            }

            if (doc.GetCellString(versionColumnIdx, i).empty())
            {
                if (index != "Expansion")
                {
                    // Except for "Expansion" heading, all other headings still get an ID associated with it
                    auto& itemType = itemUniqueItemsType[id];
                    itemType.id = id;
                    ++id;
                    itemType.index = strValue;
                    LocalizationHelpers::GetStringTxtValue(itemType.index, itemType.name);
                    itemUniqueItemsIndex[itemType.index] = itemType.id;
                }

                // skip
                continue;
            }

            if (idColumnIdx >= 0)
            {
                strValue = doc.GetCellString(idColumnIdx, i);
                if (!strValue.empty())
                {
                    id = doc.GetCellUInt16(idColumnIdx, i);
                }
            }
            auto& itemType = itemUniqueItemsType[id];
            itemType.id = id;
            ++id;
            itemType.index = index;
            LocalizationHelpers::GetStringTxtValue(itemType.index, itemType.name);
            itemUniqueItemsIndex[itemType.index] = itemType.id;

            strValue = doc.GetCellString(versionColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.version = doc.GetCellUInt16(versionColumnIdx, i);
            }

            itemType.code = doc.GetCellString(codeColumnIdx, i);
            const auto& parentItemType = GetItemTypeHelper(itemType.code);
            if (parentItemType.isQuestItem())
            {
                itemUniqueQuestItemsIndex[itemType.code] = itemType.id;
            }

            strValue = doc.GetCellString(lvlColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.level.level = doc.GetCellUInt16(lvlColumnIdx, i);
            }

            strValue = doc.GetCellString(lvlreqColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.level.levelreq = doc.GetCellUInt16(lvlreqColumnIdx, i);
            }

            itemType.inv_file = doc.GetCellString(invfileColumnIdx, i);
            itemType.transform_color = doc.GetCellString(invtransformColumnIdx, i);

            for (size_t idx = 0; idx < modParamSize; ++idx)
            {
                strValue = doc.GetCellString(modCode[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                itemType.modType.resize(itemType.modType.size() + 1);
                auto& mod = itemType.modType.back();
                mod.code = strValue;

                strValue = doc.GetCellString(modParam[idx], i);
                if (!strValue.empty())
                {
                    mod.param = strValue;
                }

                strValue = doc.GetCellString(modMin[idx], i);
                if (!strValue.empty())
                {
                    mod.min = strValue;
                }

                strValue = doc.GetCellString(modMax[idx], i);
                if (!strValue.empty())
                {
                    mod.max = strValue;
                }
            }
        }

        s_ItemUniqueQuestItemsIndex.swap(itemUniqueQuestItemsIndex);
        s_ItemUniqueItemsIndex.swap(itemUniqueItemsIndex);
        s_ItemUniqueItemsType.swap(itemUniqueItemsType);
    }

    struct ItemSetType
    {
        std::uint16_t id = 0; // index of the set

        std::string index; // string key linked to by the set field in SetItems.txt - used to tie all of the set's items to the same set.
        std::string name; // what string will be displayed in-game for this set

        //   0 = Available in Classic D2 and LoD Expansion.
        // 100 - Available in LoD Expansion only.
        std::uint16_t version = 0;

        // Different partial set modifiers a set item can grant you at most.
        std::vector<ItemAffixModType> p2ModType; // when any second set item is also equipped.
        std::vector<ItemAffixModType> p3ModType; // when any third set item is also equipped.
        std::vector<ItemAffixModType> p4ModType; // when any fourth set item is also equipped.
        std::vector<ItemAffixModType> p5ModType; // when any fifth set item is also equipped.

        std::vector<ItemAffixModType> fModType; // Full set modifiers a set item can grant you at most.

        std::vector<std::string> items; // items that belong to this set
    };

    std::map<std::string, std::uint16_t> s_ItemSetsIndex;
    std::map<std::uint16_t, ItemSetType> s_ItemSetsType;
    void InitItemSetsData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemSetsType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemSetsIndex.clear();
            s_ItemSetsType.clear();
        }

        InitItemUniqueItemsData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetSetsTxt());
        auto& doc = *pDoc;
        std::map<std::string, std::uint16_t> itemSetsIndex;
        std::map<std::uint16_t, ItemSetType> itemSetsType;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T indexColumnIdx = doc.GetColumnIdx("index");
        if (indexColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T nameColumnIdx = doc.GetColumnIdx("name");
        if (nameColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T versionColumnIdx = doc.GetColumnIdx("version");
        if (versionColumnIdx < 0)
        {
            return;
        }

        size_t pModParamSize = 8;
        std::vector<SSIZE_T> pModCode(pModParamSize, -1);
        std::vector<SSIZE_T> pModParam(pModParamSize, -1);
        std::vector<SSIZE_T> pModMin(pModParamSize, -1);
        std::vector<SSIZE_T> pModMax(pModParamSize, -1);
        for (size_t idx = 1; idx <= pModParamSize; ++idx)
        {
            std::string temp = std::to_string((idx + 3) / 2) + char('a' + (idx + 3) % 2);
            pModCode[idx - 1] = doc.GetColumnIdx("PCode" + temp);
            if (pModCode[idx - 1] < 0)
            {
                pModParamSize = idx - 1;
                pModCode.resize(pModParamSize);
                pModParam.resize(pModParamSize);
                pModMin.resize(pModParamSize);
                pModMax.resize(pModParamSize);
                break;
            }

            pModParam[idx - 1] = doc.GetColumnIdx("PParam" + temp);
            if (pModParam[idx - 1] < 0)
            {
                pModParamSize = idx - 1;
                pModCode.resize(pModParamSize);
                pModParam.resize(pModParamSize);
                pModMin.resize(pModParamSize);
                pModMax.resize(pModParamSize);
                break;
            }

            pModMin[idx - 1] = doc.GetColumnIdx("PMin" + temp);
            if (pModMin[idx - 1] < 0)
            {
                pModParamSize = idx - 1;
                pModCode.resize(pModParamSize);
                pModParam.resize(pModParamSize);
                pModMin.resize(pModParamSize);
                pModMax.resize(pModParamSize);
                break;
            }

            pModMax[idx - 1] = doc.GetColumnIdx("PMax" + temp);
            if (pModMax[idx - 1] < 0)
            {
                pModParamSize = idx - 1;
                pModCode.resize(pModParamSize);
                pModParam.resize(pModParamSize);
                pModMin.resize(pModParamSize);
                pModMax.resize(pModParamSize);
                break;
            }
        }

        size_t fModParamSize = 8;
        std::vector<SSIZE_T> fModCode(fModParamSize, -1);
        std::vector<SSIZE_T> fModParam(fModParamSize, -1);
        std::vector<SSIZE_T> fModMin(fModParamSize, -1);
        std::vector<SSIZE_T> fModMax(fModParamSize, -1);
        for (size_t idx = 1; idx <= fModParamSize; ++idx)
        {
            fModCode[idx - 1] = doc.GetColumnIdx("FCode" + std::to_string(idx));
            if (fModCode[idx - 1] < 0)
            {
                fModParamSize = idx - 1;
                fModCode.resize(fModParamSize);
                fModParam.resize(fModParamSize);
                fModMin.resize(fModParamSize);
                fModMax.resize(fModParamSize);
                break;
            }

            fModParam[idx - 1] = doc.GetColumnIdx("FParam" + std::to_string(idx));
            if (fModParam[idx - 1] < 0)
            {
                fModParamSize = idx - 1;
                fModCode.resize(fModParamSize);
                fModParam.resize(fModParamSize);
                fModMin.resize(fModParamSize);
                fModMax.resize(fModParamSize);
                break;
            }

            fModMin[idx - 1] = doc.GetColumnIdx("FMin" + std::to_string(idx));
            if (fModMin[idx - 1] < 0)
            {
                fModParamSize = idx - 1;
                fModCode.resize(fModParamSize);
                fModParam.resize(fModParamSize);
                fModMin.resize(fModParamSize);
                fModMax.resize(fModParamSize);
                break;
            }

            fModMax[idx - 1] = doc.GetColumnIdx("FMax" + std::to_string(idx));
            if (fModMax[idx - 1] < 0)
            {
                fModParamSize = idx - 1;
                fModCode.resize(fModParamSize);
                fModParam.resize(fModParamSize);
                fModMin.resize(fModParamSize);
                fModMax.resize(fModParamSize);
                break;
            }
        }

        std::string strValue;
        std::uint16_t id = 0;
        for (size_t i = 0; i < numRows; ++i)
        {
            strValue = doc.GetCellString(indexColumnIdx, i);
            if (strValue.empty())
            {
                // skip
                continue;
            }

            if (doc.GetCellString(versionColumnIdx, i).empty())
            {
                // skip
                continue;
            }

            auto& itemType = itemSetsType[id];
            itemType.id = id;
            ++id;
            itemType.index = strValue;

            strValue = doc.GetCellString(nameColumnIdx, i);
            if (strValue.empty())
            {
                LocalizationHelpers::GetStringTxtValue(itemType.index, itemType.name);
            }
            else
            {
                LocalizationHelpers::GetStringTxtValue(strValue, itemType.name);
            }
            itemSetsIndex[itemType.index] = itemType.id;

            strValue = doc.GetCellString(versionColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.version = doc.GetCellUInt16(versionColumnIdx, i);
            }

            for (size_t idx = 0; idx < pModParamSize; ++idx)
            {
                strValue = doc.GetCellString(pModCode[idx], i);
                if (strValue.empty())
                {
                    continue;
                }

                std::vector<ItemAffixModType>* pModType = nullptr;
                switch (idx / 2)
                {
                case 0:
                    pModType = &itemType.p2ModType;
                    break;

                case 1:
                    pModType = &itemType.p3ModType;
                    break;

                case 2:
                    pModType = &itemType.p4ModType;
                    break;

                case 3:
                    pModType = &itemType.p5ModType;
                    break;
                }

                if (pModType == nullptr)
                {
                    break;
                }

                pModType->resize(pModType->size() + 1);
                auto& mod = pModType->back();
                mod.code = strValue;

                strValue = doc.GetCellString(pModParam[idx], i);
                if (!strValue.empty())
                {
                    mod.param = strValue;
                }

                strValue = doc.GetCellString(pModMin[idx], i);
                if (!strValue.empty())
                {
                    mod.min = strValue;
                }

                strValue = doc.GetCellString(pModMax[idx], i);
                if (!strValue.empty())
                {
                    mod.max = strValue;
                }
            }

            for (size_t idx = 0; idx < fModParamSize; ++idx)
            {
                strValue = doc.GetCellString(fModCode[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                itemType.fModType.resize(itemType.fModType.size() + 1);
                auto& mod = itemType.fModType.back();
                mod.code = strValue;

                strValue = doc.GetCellString(fModParam[idx], i);
                if (!strValue.empty())
                {
                    mod.param = strValue;
                }

                strValue = doc.GetCellString(fModMin[idx], i);
                if (!strValue.empty())
                {
                    mod.min = strValue;
                }

                strValue = doc.GetCellString(fModMax[idx], i);
                if (!strValue.empty())
                {
                    mod.max = strValue;
                }
            }
        }

        s_ItemSetsIndex.swap(itemSetsIndex);
        s_ItemSetsType.swap(itemSetsType);
    }

    struct ItemSetItemType
    {
        std::uint16_t id = 0; // index of the set item

        std::string index; // string key to item's name
        std::string name; // what string will be displayed in-game for this set item

        std::string setIndex; // string key to the index field in Sets.txt - the set the item is a part of.

        //   0 = Available in Classic D2 and LoD Expansion.
        // 100 - Available in LoD Expansion only.
        std::uint16_t version = 0; // value from version field in Sets.txt 

        ItemAffixLevelType level; // The quality level of this set item and The character level required to use this set item
        std::string code; // The code of the base form of this set item, this is an ID pointer from Weapons.txt, Armor.txt or Misc.txt.

        std::string transform_color; // Palette shift to apply to the the DC6 inventory-file
        std::string inv_file; //  Overrides the invfile and setinvfile specified in Weapons.txt, Armor.txt or Misc.txt for the base item.

        std::vector<ItemAffixModType> modType; // Different fixed modifiers a set item can grant you (Blue attributes)

        // a property mode field that controls how the variable attributes will appear and be functional on a set item.
        // 0 -> no green properties on item (apropxx will appear as a blue attribute on the list instead).
        // 1 -> green properties (apropxx) depend on which other items from the set are equipped.
        // 2 -> green properties (apropxx) depend on how many other items from the set are equipped.
        //
        // Green attributes will appear depending on how many set items are equipped, if the add func field is either 1 or 2. If the add func field is 1, in addition to how many items, the green attributes are controlled by which other items you have equipped as 
        // well. If a set has X number of items, at most there will be X-1 green attributes on any item.
        //
        // If add func = 2, these properties will appear as below. Many of the "classic" sets in the unmodded game have this configuration.
        // a1ModType -> when any second set item is also equipped.
        // a2ModType -> when any third set item is also equipped.
        // a3ModType -> when any fourth set item is also equipped.
        // a4ModType -> when any fifth set item is also equipped.
        // a5ModType -> when any sixth set item is also equipped.
        //
        // The fun really begins when add func = 1. Different properties can emerge as different items are equipped in combination from the same set. The attributes appear for specific item pairs regardless of whether or how many other items from the set are also 
        // equipped. Civerb's Ward (shield) is the one and only example of this configuration in the unmodded game.
        // If the first item listed in the set definition is equipped, it will get the green property:
        // a1ModType -> when the second item listed in the set definition is also equipped.
        // a2ModType -> when the third item listed in the set definition is also equipped.
        // a3ModType -> when the fourth item listed in the set definition is also equipped.
        // a4ModType -> when the fifth item listed in the set definition is also equipped.
        // a5ModType -> when the sixth item listed in the set definition is also equipped.
        //
        // If the second item listed in the set definition is equipped, it will get the green property:
        // a1ModType -> when the first item listed in the set definition is also equipped.
        // a2ModType -> when the third item listed in the set definition is also equipped.
        // a3ModType -> when the fourth item listed in the set definition is also equipped.
        // a4ModType -> when the fifth item listed in the set definition is also equipped.
        // a5ModType -> when the sixth item listed in the set definition is also equipped.
        //
        // If the third item listed in the set definition is equipped, it will get the green property:
        // a1ModType -> when the first item listed in the set definition is also equipped.
        // a2ModType -> when the second item listed in the set definition is also equipped.
        // a3ModType -> when the fourth item listed in the set definition is also equipped.
        // a4ModType -> when the fifth item listed in the set definition is also equipped.
        // a5ModType -> when the sixth item listed in the set definition is also equipped.
        //
        // If the fourth item listed in the set definition is equipped, it will get the green property:
        // a1ModType -> when the first item listed in the set definition is also equipped.
        // a2ModType -> when the second item listed in the set definition is also equipped.
        // a3ModType -> when the third item listed in the set definition is also equipped.
        // a4ModType -> when the fifth item listed in the set definition is also equipped.
        // a5ModType -> when the sixth item listed in the set definition is also equipped.
        //
        // If the fifth item listed in the set definition is equipped, it will get the green property:
        // a1ModType -> when the first item listed in the set definition is also equipped.
        // a2ModType -> when the second item listed in the set definition is also equipped.
        // a3ModType -> when the third item listed in the set definition is also equipped.
        // a4ModType -> when the fourth item listed in the set definition is also equipped.
        // a5ModType -> when the sixth item listed in the set definition is also equipped.
        //
        // If the sixth item listed in the set definition is equipped, it will get the green property:
        // a1ModType -> when the first item listed in the set definition is also equipped.
        // a2ModType -> when the second item listed in the set definition is also equipped.
        // a3ModType -> when the third item listed in the set definition is also equipped.
        // a4ModType -> when the fourth item listed in the set definition is also equipped.
        // a5ModType -> when the fifth item listed in the set definition is also equipped.
        std::uint8_t addFunc = 0;

        // Different variable modifiers a set item can grant you (Green attributes)
        std::vector<ItemAffixModType> a1ModType; // when any second set item is also equipped.
        std::vector<ItemAffixModType> a2ModType; // when any second set item is also equipped.
        std::vector<ItemAffixModType> a3ModType; // when any third set item is also equipped.
        std::vector<ItemAffixModType> a4ModType; // when any fourth set item is also equipped.
        std::vector<ItemAffixModType> a5ModType; // when any fifth set item is also equipped.

        std::uint32_t dwbCode = 0; // used for items with EnumItemVersion::v100 and EnumItemVersion::v104
    };

    struct ItemRandStruct
    {
        std::uint32_t seed = 0;
        std::uint32_t carry = 0;
    };

    uint32_t GenerateRandom(ItemRandStruct& rnd)
    {
        std::uint64_t x = rnd.seed;
        x *= 0x6AC690C5;
        x += rnd.carry;
        rnd.seed = std::uint32_t(x);
        rnd.carry = std::uint32_t(x >> 32);
        return rnd.seed;
    }

    std::uint32_t InitalizeItemRandomization(std::uint32_t dwb, std::uint16_t level, EnumItemQuality quality, ItemRandStruct& rnd)
    {
        // Intialize random algorithm
        rnd.seed = dwb;
        rnd.carry = 666;

        static std::int64_t usualAdd[] = { 1000, 200, 125,  30, 12, 4 };
        static std::int64_t usualDiv[] = { 1,   1,   1,  16, 16, 8 };
        static std::int64_t excepAdd[] = { 600, 120, 100,   3,  4, 1 };
        static std::int64_t excepDiv[] = { 1,   1,   1, 100, 16, 8 };

        auto& add = quality == EnumItemQuality::SUPERIOR ? excepAdd : usualAdd;
        auto& div = quality == EnumItemQuality::SUPERIOR ? excepDiv : usualDiv;

        std::uint32_t rands = 0;
        for (size_t z = 0; z < 6; ++z)
        {
            std::uint64_t modulo = std::max(1ui64, std::uint64_t(add[z] - (level / div[z])));
            ++rands;
            if ((GenerateRandom(rnd) % modulo) == 0)
            {
                break;
            }
        }

        return rands;
    }

    std::uint32_t GenerateSetItemDWBCode(std::uint32_t itemDwbCode, std::uint16_t level)
    {
        auto dwb = ItemHelpers::generarateRandomDW();
        if (itemDwbCode == 0)
        {
            return dwb;
        }

        for (int z = 0; z < 20000; z++)
        {
            ItemRandStruct rnd = { dwb, 666 };
            InitalizeItemRandomization(dwb, level, EnumItemQuality::SET, rnd);

            std::uint32_t offset = GenerateRandom(rnd) % 0x10;
            if ((itemDwbCode & (1 << offset)) != 0)
            {
                break;
            }

            dwb = ItemHelpers::generarateRandomDW();
        }

        return dwb;
    }

    std::map<std::string, std::uint16_t> s_ItemSetItemsIndex;
    std::map<std::uint16_t, ItemSetItemType> s_ItemSetItemsType;
    void InitItemSetItemsData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemSetItemsType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemSetItemsIndex.clear();
            s_ItemSetItemsType.clear();
        }

        InitItemSetsData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetSetItemsTxt());
        auto& doc = *pDoc;
        std::map<std::string, std::uint16_t> itemSetItemsIndex;
        std::map<std::uint16_t, ItemSetItemType> itemSetItemsType;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T indexColumnIdx = doc.GetColumnIdx("index");
        if (indexColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T idColumnIdx = doc.GetColumnIdx("*ID");
        const SSIZE_T setColumnIdx = doc.GetColumnIdx("set");
        if (setColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T itemColumnIdx = doc.GetColumnIdx("item");
        if (itemColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T lvlColumnIdx = doc.GetColumnIdx("lvl");
        if (lvlColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T lvlreqColumnIdx = doc.GetColumnIdx("lvl req");
        if (lvlreqColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invtransformColumnIdx = doc.GetColumnIdx("invtransform");
        if (invtransformColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T invfileColumnIdx = doc.GetColumnIdx("invfile");
        if (invfileColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T addfuncColumnIdx = doc.GetColumnIdx("add func");
        if (addfuncColumnIdx < 0)
        {
            return;
        }

        size_t modParamSize = 9;
        std::vector<SSIZE_T> modCode(modParamSize, -1);
        std::vector<SSIZE_T> modParam(modParamSize, -1);
        std::vector<SSIZE_T> modMin(modParamSize, -1);
        std::vector<SSIZE_T> modMax(modParamSize, -1);
        for (size_t idx = 1; idx <= modParamSize; ++idx)
        {
            modCode[idx - 1] = doc.GetColumnIdx("prop" + std::to_string(idx));
            if (modCode[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }

            modParam[idx - 1] = doc.GetColumnIdx("par" + std::to_string(idx));
            if (modParam[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }

            modMin[idx - 1] = doc.GetColumnIdx("min" + std::to_string(idx));
            if (modMin[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }

            modMax[idx - 1] = doc.GetColumnIdx("max" + std::to_string(idx));
            if (modMax[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }
        }

        size_t aModParamSize = 10;
        std::vector<SSIZE_T> aModCode(aModParamSize, -1);
        std::vector<SSIZE_T> aModParam(aModParamSize, -1);
        std::vector<SSIZE_T> aModMin(aModParamSize, -1);
        std::vector<SSIZE_T> aModMax(aModParamSize, -1);
        for (size_t idx = 1; idx <= aModParamSize; ++idx)
        {
            std::string temp = std::to_string((idx + 1) / 2) + char('a' + (idx + 1) % 2);
            aModCode[idx - 1] = doc.GetColumnIdx("aprop" + temp);
            if (aModCode[idx - 1] < 0)
            {
                aModParamSize = idx - 1;
                aModCode.resize(aModParamSize);
                aModParam.resize(aModParamSize);
                aModMin.resize(aModParamSize);
                aModMax.resize(aModParamSize);
                break;
            }

            aModParam[idx - 1] = doc.GetColumnIdx("apar" + temp);
            if (aModParam[idx - 1] < 0)
            {
                aModParamSize = idx - 1;
                aModCode.resize(aModParamSize);
                aModParam.resize(aModParamSize);
                aModMin.resize(aModParamSize);
                aModMax.resize(aModParamSize);
                break;
            }

            aModMin[idx - 1] = doc.GetColumnIdx("amin" + temp);
            if (aModMin[idx - 1] < 0)
            {
                aModParamSize = idx - 1;
                aModCode.resize(aModParamSize);
                aModParam.resize(aModParamSize);
                aModMin.resize(aModParamSize);
                aModMax.resize(aModParamSize);
                break;
            }

            aModMax[idx - 1] = doc.GetColumnIdx("amax" + temp);
            if (aModMax[idx - 1] < 0)
            {
                aModParamSize = idx - 1;
                aModCode.resize(aModParamSize);
                aModParam.resize(aModParamSize);
                aModMin.resize(aModParamSize);
                aModMax.resize(aModParamSize);
                break;
            }
        }

        static std::map<std::string, std::uint16_t> setItemsDWBCode = { {"Civerb's Icon", 0x000Eui16 },
                                                                        { "Iratha's Collar", 0x0030ui16 }, { "Iratha's Cuff", 0x7FF0ui16 }, { "Iratha's Coil", 0x0070ui16 }, { "Iratha's Cord", 0x07F0ui16 },
                                                                        { "Vidala's Snare", 0x00C0ui16 },
                                                                        { "Milabrega's Diadem", 0xFF8Fui16 },
                                                                        { "Cathan's Sigil", 0x0100ui16 }, { "Cathan's Seal", 0x3F00ui16 },
                                                                        { "Tancred's Weird", 0x3E00ui16 },
                                                                        { "Infernal Sign", 0xF80Fui16 },
                                                                        { "Angelic Halo", 0xC0FFui16 }, { "Angelic Wings", 0xC000ui16 },
                                                                        { "Arctic Mitts", 0x800Fui16 },
                                                                        { "Arcanna's Sign", 0x0001ui16 } };

        std::string strValue;
        std::string index;
        std::string lastSetIndex;
        std::uint16_t id = 0;
        for (size_t i = 0; i < numRows; ++i)
        {
            index = doc.GetCellString(indexColumnIdx, i);
            if (index.empty())
            {
                // skip
                continue;
            }

            strValue = doc.GetCellString(setColumnIdx, i);
            if (strValue.empty())
            {
                // skip
                continue;
            }

            if (idColumnIdx >= 0)
            {
                strValue = doc.GetCellString(idColumnIdx, i);
                if (!strValue.empty())
                {
                    id = doc.GetCellUInt16(idColumnIdx, i);
                }
            }
            auto& itemType = itemSetItemsType[id];
            itemType.id = id;
            ++id;
            itemType.index = index;
            LocalizationHelpers::GetStringTxtValue(itemType.index, itemType.name);
            itemSetItemsIndex[itemType.index] = itemType.id;

            itemType.setIndex = strValue;
            {
                auto iter = s_ItemSetsIndex.find(itemType.setIndex);
                if (iter != s_ItemSetsIndex.end())
                {
                    auto& setInfo = s_ItemSetsType[iter->second];
                    itemType.version = setInfo.version;
                    setInfo.items.push_back(itemType.index);
                }
            }

            itemType.code = doc.GetCellString(itemColumnIdx, i);

            strValue = doc.GetCellString(lvlColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.level.level = doc.GetCellUInt16(lvlColumnIdx, i);
            }

            {
                auto iter = setItemsDWBCode.find(itemType.index);
                if (iter != setItemsDWBCode.end())
                {
                    itemType.dwbCode = GenerateSetItemDWBCode(iter->second, itemType.level.level);
                }
            }

            strValue = doc.GetCellString(lvlreqColumnIdx, i);
            if (!strValue.empty())
            {
                itemType.level.levelreq = doc.GetCellUInt16(lvlreqColumnIdx, i);
            }

            itemType.inv_file = doc.GetCellString(invfileColumnIdx, i);
            itemType.transform_color = doc.GetCellString(invtransformColumnIdx, i);

            for (size_t idx = 0; idx < modParamSize; ++idx)
            {
                strValue = doc.GetCellString(modCode[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                itemType.modType.resize(itemType.modType.size() + 1);
                auto& mod = itemType.modType.back();
                mod.code = strValue;

                strValue = doc.GetCellString(modParam[idx], i);
                if (!strValue.empty())
                {
                    mod.param = strValue;
                }

                strValue = doc.GetCellString(modMin[idx], i);
                if (!strValue.empty())
                {
                    mod.min = strValue;
                }

                strValue = doc.GetCellString(modMax[idx], i);
                if (!strValue.empty())
                {
                    mod.max = strValue;
                }
            }

            for (size_t idx = 0; idx < aModParamSize; ++idx)
            {
                strValue = doc.GetCellString(aModCode[idx], i);
                if (strValue.empty())
                {
                    continue;
                }

                std::vector<ItemAffixModType>* pModType = nullptr;
                switch (idx / 2)
                {
                case 0:
                    pModType = &itemType.a1ModType;
                    break;

                case 1:
                    pModType = &itemType.a2ModType;
                    break;

                case 2:
                    pModType = &itemType.a3ModType;
                    break;

                case 3:
                    pModType = &itemType.a4ModType;
                    break;

                case 4:
                    pModType = &itemType.a5ModType;
                    break;
                }

                if (pModType == nullptr)
                {
                    break;
                }

                pModType->resize(pModType->size() + 1);
                auto& mod = pModType->back();
                mod.code = strValue;

                strValue = doc.GetCellString(aModParam[idx], i);
                if (!strValue.empty())
                {
                    mod.param = strValue;
                }

                strValue = doc.GetCellString(aModMin[idx], i);
                if (!strValue.empty())
                {
                    mod.min = strValue;
                }

                strValue = doc.GetCellString(aModMax[idx], i);
                if (!strValue.empty())
                {
                    mod.max = strValue;
                }
            }
        }

        s_ItemSetItemsIndex.swap(itemSetItemsIndex);
        s_ItemSetItemsType.swap(itemSetItemsType);
    }

    void ProcessMagicalProperites(const std::vector<ItemAffixModType>& modTypes, std::vector<MagicalAttribute>& magicalAttributes, ItemRandStruct& rnd, d2ce::EnumItemVersion version, std::uint16_t gameVersion, bool bMaxAlways = false)
    {
        std::uint16_t func = 0;
        std::uint16_t val = 0;
        struct ChainedAttribStruct
        {
            std::uint16_t count = 0;
            std::uint16_t nextInChain = 0;
            size_t idx = 0;
        };
        std::map<std::uint16_t, ChainedAttribStruct> chainedAttribs;

        size_t numRndCalls = 0;
        for (auto& modType : modTypes)
        {
            if (modType.code.empty())
            {
                continue;
            }

            auto iter = s_ItemPropertiesType.find(modType.code);
            if (iter == s_ItemPropertiesType.end())
            {
                continue;
            }

            const auto& prop = iter->second;
            if (!prop.active)
            {
                continue;
            }

            if (prop.version > gameVersion)
            {
                if ((prop.code.compare("dmg-fire") != 0) &&
                    (prop.code.compare("dmg-ltng") != 0) &&
                    (prop.code.compare("dmg-mag") != 0) &&
                    (prop.code.compare("dmg-cold") != 0) &&
                    (prop.code.compare("dmg-pois") != 0))
                {
                    continue;
                }

                // these are just grouped properties that are supported in early versions
            }

            std::int16_t modMin = 0;
            if (!modType.min.empty())
            {
                modMin = std::int16_t(std::atoi(modType.min.c_str()));
            }

            std::int16_t modMax = 0;
            if (!modType.max.empty())
            {
                modMax = std::int16_t(std::atoi(modType.max.c_str()));
                if (modMax > modMin)
                {
                    ++numRndCalls;
                }
            }

            std::int64_t lastValue = 0;
            std::int64_t lastParam = 0;
            if (!modType.param.empty())
            {
                lastParam = std::uint16_t(std::atoi(modType.param.c_str()));
            }

            for (auto& mod : prop.parameters)
            {
                func = 0;
                if (!mod.func.empty())
                {
                    func = std::uint16_t(std::atoi(mod.func.c_str()));
                }

                MagicalAttribute attrib;
                attrib.Version = version;
                attrib.GameVersion = gameVersion;
                if (mod.stat.empty())
                {
                    switch (func)
                    {
                    case 32:
                        attrib.Id = 0;
                        attrib.Name = "Etheral";
                        attrib.Values.push_back(1);
                        magicalAttributes.push_back(attrib);
                        break;
                    }
                    continue;
                }

                auto iterName = s_ItemStatsNameMap.find(mod.stat);
                if (iterName == s_ItemStatsNameMap.end())
                {
                    continue;
                }

                val = 0;
                if (!mod.val.empty())
                {
                    val = std::uint16_t(std::atoi(mod.val.c_str()));
                }
                else
                {
                    val = std::uint16_t(lastParam);
                }

                bool bCalcValue = false;
                attrib.Id = iterName->second;
                switch (func)
                {
                case 3: // Apply the same min-max range as used in the previous function block (see res-all)
                    attrib.Values.push_back(lastValue);
                    break;

                case 9: // Apply the same param and value in min-max range, as used in the previous function block.
                    attrib.Values.push_back(lastParam);
                    attrib.Values.push_back(lastValue);
                    break;

                case 10: // skilltab skill group
                {
                    size_t classIdx = NUM_OF_CLASSES;
                    size_t tabIdx = 3;
                    GetClassAndTabId(val, classIdx, tabIdx);
                    attrib.Values.push_back(tabIdx);
                    attrib.Values.push_back(classIdx);
                    bCalcValue = true;
                    break;
                }

                case 11: // event-based skills
                    attrib.Values.push_back(modMax);
                    attrib.Values.push_back(val);
                    attrib.Values.push_back(modMin);
                    break;

                case 12: // random selection of parameters for parameter-based stat
                    if (modMax > modMin)
                    {
                        if (bMaxAlways)
                        {
                            lastValue = modMax;
                        }
                        else
                        {
                            lastValue = std::uint16_t(GenerateRandom(rnd) % (modMax - modMin) + modMin);
                        }
                        --numRndCalls;
                    }
                    else
                    {
                        lastValue = modMin;
                    }
                    attrib.Values.push_back(lastValue);
                    attrib.Values.push_back(val);
                    break;

                case 14: // inventory positions on item
                    if (modMax == 0 && modMin == 0)
                    {
                        attrib.Values.push_back(val);
                    }
                    else
                    {
                        bCalcValue = true;
                    }
                    break;

                case 15: // use min field only
                    lastValue = modMin;
                    attrib.Values.push_back(lastValue);
                    break;

                case 16: // use max field only
                    lastValue = modMax;
                    attrib.Values.push_back(lastValue);
                    break;

                case 17: // use param field only
                    attrib.Values.push_back(lastParam);
                    break;

                case 18: // Related to /time properties.
                    attrib.Values.push_back(val);
                    attrib.Values.push_back(modMax);
                    attrib.Values.push_back(modMin);
                    break;

                case 19: // Related to charged item.
                    attrib.Values.push_back(modMax);
                    attrib.Values.push_back(val);
                    attrib.Values.push_back(modMin);
                    attrib.Values.push_back(modMin);
                    break;

                case 20: // Indestructible
                    lastValue = modMin;
                    attrib.Values.push_back(lastValue);
                    break;

                case 21:
                    attrib.Values.push_back(val);
                    bCalcValue = true;
                    break;

                case 22:
                    attrib.Values.push_back(lastParam);
                    bCalcValue = true;
                    break;

                default:
                    bCalcValue = true;
                    break;
                }

                if (bCalcValue)
                {
                    if (modMax > modMin)
                    {
                        if (bMaxAlways)
                        {
                            lastValue = modMax;
                        }
                        else
                        {
                            lastValue = std::uint16_t(GenerateRandom(rnd) % (modMax - modMin) + modMin);
                        }
                        --numRndCalls;
                    }
                    else
                    {
                        lastValue = modMin;
                    }
                    attrib.Values.push_back(lastValue);
                }

                auto& stat = ItemHelpers::getItemStat(attrib);
                if (stat.id == attrib.Id)
                {
                    if (stat.nextInChain != 0)
                    {
                        auto chainedIter = chainedAttribs.find(stat.id);
                        if (chainedIter != chainedAttribs.end())
                        {
                            magicalAttributes[chainedIter->second.idx].Values[0] += attrib.Values[0];
                        }
                        else
                        {
                            attrib.Name = stat.name;
                            attrib.Desc = stat.desc;
                            attrib.Version = version;
                            attrib.GameVersion = gameVersion;
                            attrib.DescPriority = stat.descPriority;
                            attrib.encode = stat.encode;
                            magicalAttributes.push_back(attrib);
                            chainedAttribs[stat.id] = { 1, stat.nextInChain, magicalAttributes.size() - 1 };
                        }
                    }
                    else if (attrib.Id > 0)
                    {
                        auto& prevStat = ItemHelpers::getItemStat(attrib.Version, attrib.Id - 1);
                        if (prevStat.nextInChain == attrib.Id)
                        {
                            auto chainedIter = chainedAttribs.find(stat.id);
                            if (chainedIter != chainedAttribs.end())
                            {
                                // Cold and posion length are averaged
                                if (stat.name.compare("coldlength") == 0 ||
                                    stat.name.compare("poisonlength") == 0)
                                {
                                    chainedIter->second.count += 1;
                                }

                                magicalAttributes[chainedIter->second.idx].Values[0] += attrib.Values[0];
                            }
                            else
                            {
                                attrib.Name = stat.name;
                                attrib.Desc = stat.desc;
                                attrib.Version = version;
                                attrib.GameVersion = gameVersion;
                                attrib.DescPriority = stat.descPriority;
                                attrib.encode = stat.encode;
                                magicalAttributes.push_back(attrib);
                                chainedAttribs[stat.id] = { 1, 0, magicalAttributes.size() - 1 };
                            }
                        }
                        else
                        {
                            attrib.Name = stat.name;
                            attrib.Desc = stat.desc;
                            attrib.Version = version;
                            attrib.GameVersion = gameVersion;
                            attrib.DescPriority = stat.descPriority;
                            attrib.encode = stat.encode;
                            magicalAttributes.push_back(attrib);
                        }
                    }
                    else
                    {
                        attrib.Name = stat.name;
                        attrib.Desc = stat.desc;
                        attrib.Version = version;
                        attrib.GameVersion = gameVersion;
                        attrib.DescPriority = stat.descPriority;
                        attrib.encode = stat.encode;
                        magicalAttributes.push_back(attrib);
                    }
                }
            }
        }

        bool bHasChained = chainedAttribs.empty() ? false : true;
        auto iterChained = chainedAttribs.begin();
        auto iterChainedEnd = chainedAttribs.end();
        for (; iterChained != iterChainedEnd; )
        {
            auto nextInChain = iterChained->second.nextInChain;
            auto& parentAttrib = magicalAttributes[iterChained->second.idx];

            // average out any values with multiple counts
            if (iterChained->second.count > 1)
            {
                parentAttrib.Values[0] /= iterChained->second.count;
                iterChained->second.count = 1;
            }

            ++iterChained;
            if ((iterChained == iterChainedEnd) && (nextInChain == 0))
            {
                // should not happend
                continue;
            }

            for (; iterChained != iterChainedEnd && nextInChain != 0; ++iterChained)
            {
                if (iterChained->first != nextInChain)
                {
                    // should not happen
                    break;
                }

                nextInChain = iterChained->second.nextInChain;
                auto& attrib = magicalAttributes[iterChained->second.idx];
                if (iterChained->second.count > 1)
                {
                    attrib.Values[0] /= iterChained->second.count;
                    iterChained->second.count = 1;
                }

                parentAttrib.Values.push_back(attrib.Values[0]);
                attrib.clear(); // mark it for removal
            }
        }

        chainedAttribs.clear();
        if (bHasChained)
        {
            // clear out invalid items
            d2ce::removeItem_if(magicalAttributes, [](const MagicalAttribute& x) {return x.Name.empty(); });
        }

        for (size_t i = 0; i < numRndCalls; ++i)
        {
            GenerateRandom(rnd);
        }
    }

    struct ItemGemType
    {
        std::string code;   // This is a 3-letter item code for the gem
        std::string letter; // This field controls what string the game will use for the rune-letter displayed when the rune has been socketed into an item.

        std::vector<MagicalAttribute> weaponAttribs; // The modifiers a gem will give to items using GemApplyType 0 (by default this is used by weapons).
        std::vector<MagicalAttribute> helmAttribs;   // The modifiers a gem will give to items using GemApplyType 1 (by default this is used by body armors and helmets).
        std::vector<MagicalAttribute> shieldAttribs; // The modifiers a gem will give to items using GemApplyType 2 (by default this is used by shields).
    };

    std::map<std::string, ItemGemType> s_ItemGemsType;
    void InitItemGemsTypeData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemGemsType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemGemsType.clear();
        }

        InitItemSetItemsData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetGemsTxt());
        auto& doc = *pDoc;
        std::map<std::string, ItemGemType> itemGemsType;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T codeColumnIdx = doc.GetColumnIdx("code");
        if (codeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T letterColumnIdx = doc.GetColumnIdx("letter");
        if (letterColumnIdx < 0)
        {
            return;
        }

        // WARNING: Gem stats are not saved, thus having minimum and maximum assign different values will cause the gem stats to re-randomize every frame, until the gem is socketed 
        std::vector<ItemAffixModType> weaponMods; // The modifiers a gem will give to items using GemApplyType 0 (by default this is used by weapons).
        std::vector<ItemAffixModType> helmMods;   // The modifiers a gem will give to items using GemApplyType 1 (by default this is used by body armors and helmets).
        std::vector<ItemAffixModType> shieldMods; // The modifiers a gem will give to items using GemApplyType 2 (by default this is used by shields).

        size_t weaponModParamSize = 3;
        std::vector<SSIZE_T> weaponModCode(weaponModParamSize, -1);
        std::vector<SSIZE_T> weaponModParam(weaponModParamSize, -1);
        std::vector<SSIZE_T> weaponModMin(weaponModParamSize, -1);
        std::vector<SSIZE_T> weaponModMax(weaponModParamSize, -1);
        std::string columnStart;
        for (size_t idx = 1; idx <= weaponModParamSize; ++idx)
        {
            columnStart = "weaponMod" + std::to_string(idx);
            weaponModCode[idx - 1] = doc.GetColumnIdx(columnStart + "Code");
            if (weaponModCode[idx - 1] < 0)
            {
                weaponModParamSize = idx - 1;
                weaponModCode.resize(weaponModParamSize);
                weaponModParam.resize(weaponModParamSize);
                weaponModMin.resize(weaponModParamSize);
                weaponModMax.resize(weaponModParamSize);
                break;
            }

            weaponModParam[idx - 1] = doc.GetColumnIdx(columnStart + "Param");
            if (weaponModParam[idx - 1] < 0)
            {
                weaponModParamSize = idx - 1;
                weaponModCode.resize(weaponModParamSize);
                weaponModParam.resize(weaponModParamSize);
                weaponModMin.resize(weaponModParamSize);
                weaponModMax.resize(weaponModParamSize);
                break;
            }

            weaponModMin[idx - 1] = doc.GetColumnIdx(columnStart + "Min");
            if (weaponModMin[idx - 1] < 0)
            {
                weaponModParamSize = idx - 1;
                weaponModCode.resize(weaponModParamSize);
                weaponModParam.resize(weaponModParamSize);
                weaponModMin.resize(weaponModParamSize);
                weaponModMax.resize(weaponModParamSize);
                break;
            }

            weaponModMax[idx - 1] = doc.GetColumnIdx(columnStart + "Max");
            if (weaponModMax[idx - 1] < 0)
            {
                weaponModParamSize = idx - 1;
                weaponModCode.resize(weaponModParamSize);
                weaponModParam.resize(weaponModParamSize);
                weaponModMin.resize(weaponModParamSize);
                weaponModMax.resize(weaponModParamSize);
                break;
            }
        }

        size_t helmModParamSize = 3;
        std::vector<SSIZE_T> helmModCode(weaponModParamSize, -1);
        std::vector<SSIZE_T> helmModParam(weaponModParamSize, -1);
        std::vector<SSIZE_T> helmModMin(weaponModParamSize, -1);
        std::vector<SSIZE_T> helmModMax(weaponModParamSize, -1);
        for (size_t idx = 1; idx <= helmModParamSize; ++idx)
        {
            columnStart = "helmMod" + std::to_string(idx);
            helmModCode[idx - 1] = doc.GetColumnIdx(columnStart + "Code");
            if (helmModCode[idx - 1] < 0)
            {
                helmModParamSize = idx - 1;
                helmModCode.resize(helmModParamSize);
                helmModParam.resize(helmModParamSize);
                helmModMin.resize(helmModParamSize);
                helmModMax.resize(helmModParamSize);
                break;
            }

            helmModParam[idx - 1] = doc.GetColumnIdx(columnStart + "Param");
            if (helmModParam[idx - 1] < 0)
            {
                helmModParamSize = idx - 1;
                helmModCode.resize(helmModParamSize);
                helmModParam.resize(helmModParamSize);
                helmModMin.resize(helmModParamSize);
                helmModMax.resize(helmModParamSize);
                break;
            }

            helmModMin[idx - 1] = doc.GetColumnIdx(columnStart + "Min");
            if (helmModMin[idx - 1] < 0)
            {
                helmModParamSize = idx - 1;
                helmModCode.resize(helmModParamSize);
                helmModParam.resize(helmModParamSize);
                helmModMin.resize(helmModParamSize);
                helmModMax.resize(helmModParamSize);
                break;
            }

            helmModMax[idx - 1] = doc.GetColumnIdx(columnStart + "Max");
            if (helmModMax[idx - 1] < 0)
            {
                helmModParamSize = idx - 1;
                helmModCode.resize(helmModParamSize);
                helmModParam.resize(helmModParamSize);
                helmModMin.resize(helmModParamSize);
                helmModMax.resize(helmModParamSize);
                break;
            }
        }

        size_t shieldModParamSize = 3;
        std::vector<SSIZE_T> shieldModCode(weaponModParamSize, -1);
        std::vector<SSIZE_T> shieldModParam(weaponModParamSize, -1);
        std::vector<SSIZE_T> shieldModMin(weaponModParamSize, -1);
        std::vector<SSIZE_T> shieldModMax(weaponModParamSize, -1);
        for (size_t idx = 1; idx <= shieldModParamSize; ++idx)
        {
            columnStart = "shieldMod" + std::to_string(idx);
            shieldModCode[idx - 1] = doc.GetColumnIdx(columnStart + "Code");
            if (shieldModCode[idx - 1] < 0)
            {
                shieldModParamSize = idx - 1;
                shieldModCode.resize(shieldModParamSize);
                shieldModParam.resize(shieldModParamSize);
                shieldModMin.resize(shieldModParamSize);
                shieldModMax.resize(shieldModParamSize);
                break;
            }

            shieldModParam[idx - 1] = doc.GetColumnIdx(columnStart + "Param");
            if (shieldModParam[idx - 1] < 0)
            {
                shieldModParamSize = idx - 1;
                shieldModCode.resize(shieldModParamSize);
                shieldModParam.resize(shieldModParamSize);
                shieldModMin.resize(shieldModParamSize);
                shieldModMax.resize(shieldModParamSize);
                break;
            }

            shieldModMin[idx - 1] = doc.GetColumnIdx(columnStart + "Min");
            if (shieldModMin[idx - 1] < 0)
            {
                shieldModParamSize = idx - 1;
                shieldModCode.resize(shieldModParamSize);
                shieldModParam.resize(shieldModParamSize);
                shieldModMin.resize(shieldModParamSize);
                shieldModMax.resize(shieldModParamSize);
                break;
            }

            shieldModMax[idx - 1] = doc.GetColumnIdx(columnStart + "Max");
            if (shieldModMax[idx - 1] < 0)
            {
                shieldModParamSize = idx - 1;
                shieldModCode.resize(shieldModParamSize);
                shieldModParam.resize(shieldModParamSize);
                shieldModMin.resize(shieldModParamSize);
                shieldModMax.resize(shieldModParamSize);
                break;
            }
        }

        // randomizer should not be needed as min/max should be equal, but just in case we create one and pass it in.
        auto dwb = ItemHelpers::generarateRandomDW();
        ItemRandStruct rnd = { dwb, 666 };
        InitalizeItemRandomization(dwb, 0, EnumItemQuality::NORMAL, rnd);

        std::string strValue;
        for (size_t i = 0; i < numRows; ++i)
        {
            strValue = doc.GetCellString(codeColumnIdx, i);
            if (strValue.empty())
            {
                // skip
                continue;
            }

            auto& itemType = itemGemsType[strValue];
            itemType.code = strValue;
            LocalizationHelpers::GetStringTxtValue(doc.GetCellString(letterColumnIdx, i), itemType.letter);

            weaponMods.clear();
            for (size_t idx = 0; idx < weaponModParamSize; ++idx)
            {
                strValue = doc.GetCellString(weaponModCode[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                weaponMods.resize(weaponMods.size() + 1);
                auto& mod = weaponMods.back();
                mod.code = strValue;

                strValue = doc.GetCellString(weaponModParam[idx], i);
                if (!strValue.empty())
                {
                    mod.param = strValue;
                }

                strValue = doc.GetCellString(weaponModMin[idx], i);
                if (!strValue.empty())
                {
                    mod.min = strValue;
                }

                strValue = doc.GetCellString(weaponModMax[idx], i);
                if (!strValue.empty())
                {
                    mod.max = strValue;
                }
            }

            helmMods.clear();
            for (size_t idx = 0; idx < helmModParamSize; ++idx)
            {
                strValue = doc.GetCellString(helmModCode[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                helmMods.resize(helmMods.size() + 1);
                auto& mod = helmMods.back();
                mod.code = strValue;

                strValue = doc.GetCellString(helmModParam[idx], i);
                if (!strValue.empty())
                {
                    mod.param = strValue;
                }

                strValue = doc.GetCellString(helmModMin[idx], i);
                if (!strValue.empty())
                {
                    mod.min = strValue;
                }

                strValue = doc.GetCellString(helmModMax[idx], i);
                if (!strValue.empty())
                {
                    mod.max = strValue;
                }
            }

            shieldMods.clear();
            for (size_t idx = 0; idx < shieldModParamSize; ++idx)
            {
                strValue = doc.GetCellString(shieldModCode[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                shieldMods.resize(shieldMods.size() + 1);
                auto& mod = shieldMods.back();
                mod.code = strValue;

                strValue = doc.GetCellString(shieldModParam[idx], i);
                if (!strValue.empty())
                {
                    mod.param = strValue;
                }

                strValue = doc.GetCellString(shieldModMin[idx], i);
                if (!strValue.empty())
                {
                    mod.min = strValue;
                }

                strValue = doc.GetCellString(shieldModMax[idx], i);
                if (!strValue.empty())
                {
                    mod.max = strValue;
                }
            }

            ProcessMagicalProperites(weaponMods, itemType.weaponAttribs, rnd, APP_ITEM_VERSION, APP_ITEM_GAME_VERSION);
            ProcessMagicalProperites(helmMods, itemType.helmAttribs, rnd, APP_ITEM_VERSION, APP_ITEM_GAME_VERSION);
            ProcessMagicalProperites(shieldMods, itemType.shieldAttribs, rnd, APP_ITEM_VERSION, APP_ITEM_GAME_VERSION);
        }

        s_ItemGemsType.swap(itemGemsType);
    }

    std::map<std::uint16_t, RunewordType> s_ItemRunewordsType;
    std::map<std::uint8_t, std::vector<std::uint16_t>> s_ItemNumRunesRunewordsMap;
    void InitRunewordData(const ITxtReader& txtReader)
    {
        static const ITxtReader* pCurTextReader = nullptr;
        if (!s_ItemRunewordsType.empty())
        {
            if (pCurTextReader == &txtReader)
            {
                // already initialized
                return;
            }

            s_ItemNumRunesRunewordsMap.clear();
            s_ItemRunewordsType.clear();
        }

        InitItemGemsTypeData(txtReader);
        pCurTextReader = &txtReader;
        auto pDoc(txtReader.GetRunesTxt());
        auto& doc = *pDoc;
        std::map<std::uint16_t, RunewordType> itemRunewordsType;
        std::map<std::uint8_t, std::vector<std::uint16_t>> itemNumRunesRunewordsMap;
        size_t numRows = doc.GetRowCount();
        const SSIZE_T indexColumnIdx = doc.GetColumnIdx("Name");
        if (indexColumnIdx < 0)
        {
            return;
        }

        SSIZE_T nameColumnIdx = doc.GetColumnIdx("*Rune Name");
        if (nameColumnIdx < 0)
        {
            nameColumnIdx = doc.GetColumnIdx("Rune Name");
            if (nameColumnIdx < 0)
            {
                return;
            }
        }


        SSIZE_T patchReleaseColumnIdx = doc.GetColumnIdx("*Patch Release"); // optional
        const SSIZE_T completeColumnIdx = doc.GetColumnIdx("complete");
        if (completeColumnIdx < 0)
        {
            return;
        }

        const SSIZE_T serverColumnIdx = doc.GetColumnIdx("server");
        if (serverColumnIdx < 0)
        {
            return;
        }

        size_t itypesParamSize = 5;
        std::vector<SSIZE_T> itypes(itypesParamSize, -1);
        for (size_t idx = 1; idx <= itypesParamSize; ++idx)
        {
            itypes[idx - 1] = doc.GetColumnIdx("itype" + std::to_string(idx));
            if (itypes[idx - 1] < 0)
            {
                itypesParamSize = idx - 1;
                itypes.resize(itypesParamSize);
                break;
            }
        }

        size_t etypesParamSize = 3;
        std::vector<SSIZE_T> etypes(etypesParamSize, -1);
        for (size_t idx = 1; idx <= etypesParamSize; ++idx)
        {
            etypes[idx - 1] = doc.GetColumnIdx("etype" + std::to_string(idx));
            if (etypes[idx - 1] < 0)
            {
                etypesParamSize = idx - 1;
                etypes.resize(etypesParamSize);
                break;
            }
        }

        size_t runesParamSize = 6;
        std::vector<SSIZE_T> runes(runesParamSize, -1);
        for (size_t idx = 1; idx <= runesParamSize; ++idx)
        {
            runes[idx - 1] = doc.GetColumnIdx("Rune" + std::to_string(idx));
            if (runes[idx - 1] < 0)
            {
                runesParamSize = idx - 1;
                runes.resize(runesParamSize);
                break;
            }
        }

        std::vector<ItemAffixModType> mods;
        size_t modParamSize = 7;
        std::vector<SSIZE_T> modCode(modParamSize, -1);
        std::vector<SSIZE_T> modParam(modParamSize, -1);
        std::vector<SSIZE_T> modMin(modParamSize, -1);
        std::vector<SSIZE_T> modMax(modParamSize, -1);
        for (size_t idx = 1; idx <= modParamSize; ++idx)
        {
            modCode[idx - 1] = doc.GetColumnIdx("T1Code" + std::to_string(idx));
            if (modCode[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }

            modParam[idx - 1] = doc.GetColumnIdx("T1Param" + std::to_string(idx));
            if (modParam[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }

            modMin[idx - 1] = doc.GetColumnIdx("T1Min" + std::to_string(idx));
            if (modMin[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }

            modMax[idx - 1] = doc.GetColumnIdx("T1Max" + std::to_string(idx));
            if (modMax[idx - 1] < 0)
            {
                modParamSize = idx - 1;
                modCode.resize(modParamSize);
                modParam.resize(modParamSize);
                modMin.resize(modParamSize);
                modMax.resize(modParamSize);
                break;
            }
        }

        // randomizer should not be needed as min/max should be equal, but just in case we create one and pass it in.
        auto dwb = ItemHelpers::generarateRandomDW();
        ItemRandStruct rnd = { dwb, 666 };
        InitalizeItemRandomization(dwb, 0, EnumItemQuality::NORMAL, rnd);

        std::string strValue;
        std::string index;
        std::uint16_t id = 0;
        for (size_t i = 0; i < numRows; ++i)
        {
            strValue = doc.GetCellString(completeColumnIdx, i);
            if (strValue.empty() || (strValue == "0"))
            {
                // skip, not available to use
                continue;
            }

            index = doc.GetCellString(indexColumnIdx, i);
            if (index.length() < 9)
            {
                // skip
                continue;
            }

            id = static_cast<std::uint16_t>(std::stol(index.substr(8)));
            if (id == 0)
            {
                // skip
                continue;
            }

            if (id < 80)
            {
                id += 26; // first runeword is ID 27
            }
            else
            {
                // runeword80 doesn't exist, so skip it
                id += 25;
            }

            auto& itemType = itemRunewordsType[id];
            itemType.id = id;

            strValue = doc.GetCellString(nameColumnIdx, i);
            LocalizationHelpers::GetStringTxtValue(index, itemType.name, strValue.c_str());
            bool fixupDelirium = false;
            if (id == 48 && strValue == "Delirium")
            {
                fixupDelirium = true;
            }

            strValue = doc.GetCellString(serverColumnIdx, i);
            if (strValue == "1")
            {
                itemType.serverOnly = true;
            }

            for (size_t idx = 0; idx < itypesParamSize; ++idx)
            {
                strValue = doc.GetCellString(itypes[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                auto& cat = GetItemCategory(strValue);
                if (cat.code != strValue)
                {
                    break;
                }

                itemType.included_categories.push_back(cat.name);
            }

            for (size_t idx = 0; idx < etypesParamSize; ++idx)
            {
                strValue = doc.GetCellString(etypes[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                auto& cat = GetItemCategory(strValue);
                if (cat.code != strValue)
                {
                    break;
                }

                itemType.excluded_categories.push_back(cat.name);
            }

            itemType.runeCodes.clear();
            auto iterMiscType = s_ItemMiscType.end();
            for (size_t idx = 0; idx < runesParamSize; ++idx)
            {
                strValue = doc.GetCellString(runes[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                itemType.runeCodes.push_back(strValue);
                iterMiscType = s_ItemMiscType.find(strValue);
                if (iterMiscType != s_ItemMiscType.end())
                {
                    itemType.levelreq = std::max(itemType.levelreq, iterMiscType->second.req.Level);
                }
            }

            mods.clear();
            for (size_t idx = 0; idx < modParamSize; ++idx)
            {
                strValue = doc.GetCellString(modCode[idx], i);
                if (strValue.empty())
                {
                    break;
                }

                mods.resize(mods.size() + 1);
                auto& mod = mods.back();
                mod.code = strValue;

                strValue = doc.GetCellString(modParam[idx], i);
                if (!strValue.empty())
                {
                    auto c = strValue[0];
                    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
                    {
                        // this must be a skill index name
                        const auto& skillInfo = CharClassHelper::getSkillByIndex(strValue);
                        if (skillInfo.id != MAXUINT16)
                        {
                            // convert to a skill ID
                            strValue = std::to_string(skillInfo.id);
                        }
                    }

                    mod.param = strValue;
                }

                strValue = doc.GetCellString(modMin[idx], i);
                if (!strValue.empty())
                {
                    mod.min = strValue;
                }

                strValue = doc.GetCellString(modMax[idx], i);
                if (!strValue.empty())
                {
                    mod.max = strValue;
                }
            }

            ProcessMagicalProperites(mods, itemType.attribs, rnd, APP_ITEM_VERSION, APP_ITEM_GAME_VERSION, true);
            itemNumRunesRunewordsMap[std::uint8_t(itemType.runeCodes.size())].push_back(itemType.id);

            static std::map<std::string, d2ce::EnumItemVersion> patchReleaseMap = {
                {"Runeword1", d2ce::EnumItemVersion::v109}, {"Runeword4", d2ce::EnumItemVersion::v110},
                {"Runeword6", d2ce::EnumItemVersion::v109}, {"Runeword8", d2ce::EnumItemVersion::v110},
                {"Runeword9", d2ce::EnumItemVersion::v110}, {"Runeword10", d2ce::EnumItemVersion::v110},
                {"Runeword11", d2ce::EnumItemVersion::v110}, {"Runeword13", d2ce::EnumItemVersion::v110},
                {"Runeword14", d2ce::EnumItemVersion::v110}, {"Runeword15", d2ce::EnumItemVersion::v110},
                {"Runeword17", d2ce::EnumItemVersion::v110}, {"Runeword18", d2ce::EnumItemVersion::v110},
                {"Runeword20", d2ce::EnumItemVersion::v110}, {"Runeword22", d2ce::EnumItemVersion::v110},
                {"Runeword25", d2ce::EnumItemVersion::v110}, {"Runeword26", d2ce::EnumItemVersion::v110},
                {"Runeword27", d2ce::EnumItemVersion::v110}, {"Runeword29", d2ce::EnumItemVersion::v110},
                {"Runeword30", d2ce::EnumItemVersion::v110}, {"Runeword31", d2ce::EnumItemVersion::v110},
                {"Runeword33", d2ce::EnumItemVersion::v110}, {"Runeword34", d2ce::EnumItemVersion::v110},
                {"Runeword36", d2ce::EnumItemVersion::v110}, {"Runeword37", d2ce::EnumItemVersion::v110},
                {"Runeword38", d2ce::EnumItemVersion::v110}, {"Runeword40", d2ce::EnumItemVersion::v116},
                {"Runeword41", d2ce::EnumItemVersion::v110}, {"Runeword44", d2ce::EnumItemVersion::v109},
                {"Runeword45", d2ce::EnumItemVersion::v110}, {"Runeword47", d2ce::EnumItemVersion::v110}, 
                {"Runeword48", d2ce::EnumItemVersion::v110}, {"Runeword49", d2ce::EnumItemVersion::v110},
                {"Runeword51", d2ce::EnumItemVersion::v110}, {"Runeword54", d2ce::EnumItemVersion::v109}, 
                {"Runeword55", d2ce::EnumItemVersion::v109}, {"Runeword59", d2ce::EnumItemVersion::v110}, 
                {"Runeword60", d2ce::EnumItemVersion::v110}, {"Runeword62", d2ce::EnumItemVersion::v110},
                {"Runeword65", d2ce::EnumItemVersion::v109}, {"Runeword66", d2ce::EnumItemVersion::v110},
                {"Runeword69", d2ce::EnumItemVersion::v110}, {"Runeword71", d2ce::EnumItemVersion::v110},
                {"Runeword72", d2ce::EnumItemVersion::v109}, {"Runeword74", d2ce::EnumItemVersion::v109},
                {"Runeword75", d2ce::EnumItemVersion::v109}, {"Runeword81", d2ce::EnumItemVersion::v109},
                {"Runeword82", d2ce::EnumItemVersion::v109}, {"Runeword83", d2ce::EnumItemVersion::v109},
                {"Runeword84", d2ce::EnumItemVersion::v116}, {"Runeword87", d2ce::EnumItemVersion::v110},
                {"Runeword88", d2ce::EnumItemVersion::v109}, {"Runeword91", d2ce::EnumItemVersion::v110},
                {"Runeword92", d2ce::EnumItemVersion::v110}, {"Runeword94", d2ce::EnumItemVersion::v116},
                {"Runeword95", d2ce::EnumItemVersion::v110}, {"Runeword97", d2ce::EnumItemVersion::v116},
                {"Runeword98", d2ce::EnumItemVersion::v110}, {"Runeword99", d2ce::EnumItemVersion::v110},
                {"Runeword103", d2ce::EnumItemVersion::v110}, {"Runeword106", d2ce::EnumItemVersion::v116},
                {"Runeword109", d2ce::EnumItemVersion::v110}, {"Runeword110", d2ce::EnumItemVersion::v110},
                {"Runeword112", d2ce::EnumItemVersion::v110}, {"Runeword116", d2ce::EnumItemVersion::v109},
                {"Runeword117", d2ce::EnumItemVersion::v110}, {"Runeword120", d2ce::EnumItemVersion::v109},
                {"Runeword121", d2ce::EnumItemVersion::v110}, {"Runeword122", d2ce::EnumItemVersion::v110},
                {"Runeword130", d2ce::EnumItemVersion::v110}, {"Runeword126", d2ce::EnumItemVersion::v109},
                {"Runeword128", d2ce::EnumItemVersion::v109}, {"Runeword131", d2ce::EnumItemVersion::v110},
                {"Runeword133", d2ce::EnumItemVersion::v109}, {"Runeword134", d2ce::EnumItemVersion::v109},
                {"Runeword137", d2ce::EnumItemVersion::v110}, {"Runeword139", d2ce::EnumItemVersion::v109},
                {"Runeword148", d2ce::EnumItemVersion::v110}, {"Runeword151", d2ce::EnumItemVersion::v116},
                {"Runeword154", d2ce::EnumItemVersion::v109}, {"Runeword160", d2ce::EnumItemVersion::v109},
                {"Runeword162", d2ce::EnumItemVersion::v109}, {"Runeword163", d2ce::EnumItemVersion::v110},
                {"Runeword165", d2ce::EnumItemVersion::v116}, {"Runeword168", d2ce::EnumItemVersion::v110},
                {"Runeword170", d2ce::EnumItemVersion::v109},
            };

            if (patchReleaseColumnIdx >= 0)
            {
                strValue = doc.GetCellString(patchReleaseColumnIdx, i);
                if(strValue == "109")
                {
                    itemType.version = d2ce::EnumItemVersion::v109;
                }
                else if ((strValue.find("110") == 0) | (strValue == "111") || (strValue == "Previously Ladder Only"))
                {
                    itemType.version = d2ce::EnumItemVersion::v110;
                }
                else if (strValue.find("D2R Ladder 1") == 0)
                {
                    itemType.version = d2ce::EnumItemVersion::v116;
                }
                else
                {
                    auto iterVersion = patchReleaseMap.find(index);
                    if (iterVersion != patchReleaseMap.end())
                    {
                        itemType.version = iterVersion->second;
                    }
                }
            }
            else
            {
                auto iterVersion = patchReleaseMap.find(index);
                if (iterVersion != patchReleaseMap.end())
                {
                    itemType.version = iterVersion->second;
                }
            }

            if (fixupDelirium)
            {
                // Delirium is marked as ID 2718 and not 48
                itemRunewordsType[2718] = itemType;
            }
        }

        s_ItemRunewordsType.swap(itemRunewordsType);
        s_ItemNumRunesRunewordsMap.swap(itemNumRunesRunewordsMap);
    }

    void InitItemData(const ITxtReader& txtReader)
    {
        InitItemStatsData(txtReader);
        InitItemRarePrefixData(txtReader);
        InitItemGemsTypeData(txtReader);
    }

    const std::map<std::string, std::uint8_t> huffmanDecodeMap = {
        {"111101000", '\0'}, {       "01", ' '}, {"11011111", '0'}, { "0011111", '1'},
        {   "001100",  '2'}, {  "1011011", '3'}, {"01011111", '4'}, {"01101000", '5'},
        {  "1111011",  '6'}, {    "11110", '7'}, {  "001000", '8'}, {   "01110", '9'},
        {    "01111",  'a'}, {     "1010", 'b'}, {   "00010", 'c'}, {  "100011", 'd'},
        {   "000011",  'e'}, {   "110010", 'f'}, {   "01011", 'g'}, {   "11000", 'h'},
        {  "0111111",  'i'}, {"011101000", 'j'}, {  "010010", 'k'}, {   "10111", 'l'},
        {    "10110",  'm'}, {   "101100", 'n'}, { "1111111", 'o'}, {   "10011", 'p'},
        { "10011011",  'q'}, {    "00111", 'r'}, {    "0100", 's'}, {   "00110", 't'},
        {    "10000",  'u'}, {  "0111011", 'v'}, {   "00000", 'w'}, {   "11100", 'x'},
        {  "0101000",  'y'}, { "00011011", 'z'},
    };

    // Retrieves the huffman encoded chracter
    std::uint8_t GetEncodedChar(const std::vector<std::uint8_t>& data, std::uint64_t& startOffset)
    {
        std::string bitStr;
        size_t startRead = startOffset;
        size_t readOffset = startRead;
        while (bitStr.size() < 9)
        {
            readOffset = startRead;
            std::stringstream ss2;
            ss2 << std::bitset<9>(read_uint32_bits(readOffset, bitStr.size() + 1));
            ++startOffset;
            bitStr = ss2.str().substr(8 - bitStr.size());
            auto iter = huffmanDecodeMap.find(bitStr);
            if (iter != huffmanDecodeMap.end())
            {
                return iter->second;
            }
        }

        // something went wrong
        return std::uint8_t(0xFF);
    }

    const ItemSetItemType& GetSetItemType(std::uint16_t id, const std::array<std::uint8_t, 4>& strcode)
    {
        static ItemSetItemType badValue;
        auto iterSets = s_ItemSetsType.find(id);
        if (iterSets == s_ItemSetsType.end())
        {
            return badValue;
        }

        // find set item
        std::string testStr("   ");
        testStr[0] = (char)strcode[0];
        testStr[1] = (char)strcode[1];
        testStr[2] = (char)strcode[2];
        for (const auto& setItem : iterSets->second.items)
        {
            auto iter = s_ItemSetItemsIndex.find(setItem);
            if (iter == s_ItemSetItemsIndex.end())
            {
                // should not happen
                continue;
            }

            const auto& setItemType = s_ItemSetItemsType[iter->second];
            if (setItemType.code == testStr)
            {
                return setItemType;
            }
        }

        return badValue;
    }

    bool GenerateMagicalAffixesBuffer(const std::array<std::uint8_t, 4>& strcode, std::vector<ItemAffixType>& prefixes, std::vector<ItemAffixType>& suffixes, std::uint16_t gameVersion, std::uint16_t level)
    {
        prefixes.clear();
        suffixes.clear();

        const auto& itemType = GetItemTypeHelper(strcode);
        if (&itemType == &s_invalidItemType)
        {
            return false;
        }

        for (auto& prefix : s_ItemMagicPrefixType)
        {
            if (prefix.second.version > gameVersion)
            {
                continue;
            }

            if (prefix.second.level.level - 2 > level)
            {
                continue;
            }

            if (prefix.second.level.maxLevel != 0 && prefix.second.level.maxLevel < level)
            {
                continue;
            }

            bool bIncluded = false;
            for (auto& itype : prefix.second.included_categories)
            {
                if (itemType.hasCategory(itype))
                {
                    bIncluded = true;
                    break;
                }
            }

            if (!bIncluded)
            {
                continue;
            }

            bool bExcluded = false;
            for (auto& etype : prefix.second.excluded_categories)
            {
                if (itemType.hasCategory(etype))
                {
                    bExcluded = true;
                    break;
                }
            }

            if (bExcluded)
            {
                continue;
            }

            prefixes.push_back(prefix.second);
        }

        for (auto& suffix : s_ItemMagicSuffixType)
        {
            if (suffix.second.version > gameVersion)
            {
                continue;
            }

            if (suffix.second.level.level - 2 > level)
            {
                continue;
            }

            bool bIncluded = false;
            for (auto& itype : suffix.second.included_categories)
            {
                if (itemType.hasCategory(itype))
                {
                    bIncluded = true;
                    break;
                }
            }

            if (!bIncluded)
            {
                continue;
            }

            bool bExcluded = false;
            for (auto& etype : suffix.second.excluded_categories)
            {
                if (itemType.hasCategory(etype))
                {
                    bExcluded = true;
                    break;
                }
            }

            if (bExcluded)
            {
                continue;
            }

            suffixes.push_back(suffix.second);
        }
        return true;
    }

    bool GenerateRareOrCraftedAffixesBuffer(const std::array<std::uint8_t, 4>& strcode, std::vector<ItemAffixType>& prefixes, std::vector<ItemAffixType>& suffixes, std::uint16_t gameVersion)
    {
        prefixes.clear();
        suffixes.clear();

        const auto& itemType = GetItemTypeHelper(strcode);
        if (&itemType == &s_invalidItemType)
        {
            return false;
        }

        for (auto& prefix : s_ItemRarePrefixType)
        {
            if (prefix.second.version > gameVersion)
            {
                continue;
            }

            bool bIncluded = false;
            for (auto& itype : prefix.second.included_categories)
            {
                if (itemType.hasCategory(itype))
                {
                    bIncluded = true;
                    break;
                }
            }

            if (!bIncluded)
            {
                continue;
            }

            bool bExcluded = false;
            for (auto& etype : prefix.second.excluded_categories)
            {
                if (itemType.hasCategory(etype))
                {
                    bExcluded = true;
                    break;
                }
            }

            if (bExcluded)
            {
                continue;
            }

            prefixes.push_back(prefix.second);
        }

        for (auto& suffix : s_ItemRareSuffixType)
        {
            if (suffix.second.version > gameVersion)
            {
                continue;
            }

            bool bIncluded = false;
            for (auto& itype : suffix.second.included_categories)
            {
                if (itemType.hasCategory(itype))
                {
                    bIncluded = true;
                    break;
                }
            }

            if (!bIncluded)
            {
                continue;
            }

            bool bExcluded = false;
            for (auto& etype : suffix.second.excluded_categories)
            {
                if (itemType.hasCategory(etype))
                {
                    bExcluded = true;
                    break;
                }
            }

            if (bExcluded)
            {
                continue;
            }

            suffixes.push_back(suffix.second);
        }
        return true;
    }
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::hasCategory(const std::string category) const
{
    return std::find(categories.begin(), categories.end(), category) != categories.end() ? true : false;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::hasCategoryCode(const std::string categoryCode) const
{
    const auto& category = GetItemCategory(categoryCode);
    if (category.name.empty())
    {
        if (categoryCode == "sppl")
        {
            // support legacy text files Spears and Polearms
            const auto& categorySpear = GetItemCategory("spea");
            if (categorySpear.name.empty())
            {
                return false; // should not happen
            }

            if (hasCategory(categorySpear.name))
            {
                return true;
            }

            const auto& categoryPole = GetItemCategory("pole");
            if (categoryPole.name.empty())
            {
                return false; // should not happen
            }

            return hasCategory(categoryPole.name);
        }

        if (categoryCode == "blde")
        {
            // support legacy text files Swords and Knives
            const auto& categorySwor = GetItemCategory("swor");
            if (categorySwor.name.empty())
            {
                return false; // should not happen
            }

            if (hasCategory(categorySwor.name))
            {
                return true;
            }

            const auto& categoryKnif = GetItemCategory("knif");
            if (categoryKnif.name.empty())
            {
                return false; // should not happen
            }

            return hasCategory(categoryKnif.name);
        }
    
        return false;
    }

    return hasCategory(category.name);
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isStackable() const
{
    return stackable.Max > 0 ? true : false;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isWeapon() const
{
    return hasCategoryCode("weap");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isThrownWeapon() const
{
    return hasCategoryCode("thro");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isMissileWeapon() const
{
    return hasCategoryCode("miss");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isMissile() const
{
    return hasCategoryCode("misl");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isTwoHandedWeapon() const
{
    if (!isWeapon())
    {
        return false;
    }

    return dam.bTwoHanded;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isOneOrTwoHandedWeapon() const
{
    if (!isWeapon())
    {
        return false;
    }

    return dam.bTwoHanded ? dam.bOneOrTwoHanded : true;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isShield() const
{
    return hasCategoryCode("shld");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isArmor() const
{
    return hasCategoryCode("armo");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isHelm() const
{
    return hasCategoryCode("helm");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isMiscellaneous() const
{
    return hasCategoryCode("misc");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isBook() const
{
    return hasCategoryCode("book");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isPotion() const
{
    return hasCategoryCode("poti");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isJewel() const
{
    return hasCategoryCode("jewl");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isGem() const
{
    return hasCategoryCode("gem");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isQuestItem() const
{
    return hasCategoryCode("ques");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isGoldItem() const
{
    return hasCategoryCode("gold");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isRejuvenationPotion() const
{
    return hasCategoryCode("rpot");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isHealingPotion() const
{
    return hasCategoryCode("hpot") ? !isRejuvenationPotion() : false;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isManaPotion() const
{
    return hasCategoryCode("mpot") ? !isRejuvenationPotion() : false;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isSocketFiller() const
{
    return hasCategoryCode("sock");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isUpgradableGem() const
{
    if (isSocketFiller())
    {
        return (!isGem() || hasCategoryCode("gem4")) ? false : true;
    }

    return false;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isUpgradableRejuvenationPotion() const
{
    if (isRejuvenationPotion())
    {
        return code.compare("rvl") == 0 ? false : true;
    }

    return false;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isUpgradablePotion() const
{
    if (isRejuvenationPotion())
    {
        return isUpgradableRejuvenationPotion();
    }

    if (isPotion())
    {
        if (code.empty())
        {
            return false;
        }

        return code.back() == '5' ? false : true;
    }

    return false;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isRune() const
{
    return hasCategoryCode("rune");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isCharm() const
{
    return hasCategoryCode("char");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isBelt() const
{
    return hasCategoryCode("belt");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isBoots() const
{
    return hasCategoryCode("boot");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isBeltable() const
{
    return beltable;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isScroll() const
{
    return hasCategoryCode("scro");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isKey() const
{
    return hasCategoryCode("key");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isHoradricCube() const
{
    return code.compare("box") == 0 ? true : false;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isRing() const
{
    return hasCategoryCode("ring");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isAmulet() const
{
    return hasCategoryCode("amul");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isBodyPart() const
{
    return hasCategoryCode("body");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isSimpleItem() const
{
    return compactsave;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isUnusedItem() const
{
    return hasCategory("Unused");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isExpansionItem() const
{
    return version != 0 ? true : false;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isClassSpecific() const
{
    return hasCategoryCode("clas");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::isSecondHand() const
{
    return hasCategoryCode("seco");
}
//---------------------------------------------------------------------------
std::optional<d2ce::EnumCharClass> d2ce::ItemType::getClass() const
{
    if (isClassSpecific())
    {
        if (hasCategoryCode("amaz"))
        {
            return EnumCharClass::Amazon;
        }

        if (hasCategoryCode("amaz"))
        {
            return EnumCharClass::Amazon;
        }

        if (hasCategoryCode("barb"))
        {
            return EnumCharClass::Barbarian;
        }

        if (hasCategoryCode("necr"))
        {
            return EnumCharClass::Necromancer;
        }

        if (hasCategoryCode("pala"))
        {
            return EnumCharClass::Paladin;
        }

        if (hasCategoryCode("sorc"))
        {
            return EnumCharClass::Sorceress;
        }

        if (hasCategoryCode("assn"))
        {
            return EnumCharClass::Assassin;
        }

        if (hasCategoryCode("drui"))
        {
            return EnumCharClass::Druid;
        }
    }

    return std::optional<d2ce::EnumCharClass>();
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::hasUndeadBonus() const
{
    if (!isWeapon())
    {
        return false;
    }
    return hasCategoryCode("blun");
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::canHaveSockets() const
{
    return max_sockets.empty() ? false : true;
}
//---------------------------------------------------------------------------
std::uint8_t d2ce::ItemType::getMaxSockets(std::uint8_t lvl) const
{
    if (max_sockets.empty())
    {
        return 0;
    }

    auto iter = max_sockets.begin();
    if (iter->first >= lvl)
    {
        return iter->second;
    }

    auto iter_end = max_sockets.end();
    auto lastIter = iter;
    ++iter;
    for (; iter != iter_end; ++iter)
    {
        if (iter->first > lvl)
        {
            return iter->second;
        }
        lastIter = iter;
    }

    return lastIter->second;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::canPersonalize() const
{
    return nameable;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::canEquip(EnumEquippedId equipId) const
{
    switch (equipId)
    {
    case EnumEquippedId::ALT_RIGHT_ARM:
        equipId = EnumEquippedId::RIGHT_ARM;
        break;

    case EnumEquippedId::ALT_LEFT_ARM:
        equipId = EnumEquippedId::LEFT_ARM;
        break;
    }

    return (std::find(bodyLocations.begin(), bodyLocations.end(), equipId) != bodyLocations.end()) ? true : false;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::canEquip(EnumEquippedId equipId, EnumCharClass charClass) const
{
    if (!canEquip(equipId))
    {
        return false;
    }

    auto itemCharClass = getClass();
    if (!itemCharClass.has_value())
    {
        // not class specific so we are ok
        return true;
    }

    return (charClass == itemCharClass.value()) ? true : false;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::getSocketedMagicalAttributes(const d2ce::Item& item, std::vector<MagicalAttribute>& attribs, std::uint8_t parentGemApplyType) const
{
    attribs.clear();
    if (isSocketFiller())
    {
        if (isJewel())
        {
            return item.getMagicalAttributes(attribs);
        }

        if (!isGem() && !isRune())
        {
            return false;
        }

        auto iter = s_ItemGemsType.find(code);
        if (iter == s_ItemGemsType.end())
        {
            // should not happen
            return false;
        }

        switch (parentGemApplyType)
        {
        case 0:
            attribs = iter->second.weaponAttribs;
            break;

        case 1:
            attribs = iter->second.helmAttribs;
            break;

        case 2:
            attribs = iter->second.shieldAttribs;
            break;

        default:
            return false;
        }

        auto itemVersion = item.getVersion();
        auto gameVersion = item.getGameVersion();
        for (auto& attrib : attribs)
        {
            attrib.Version = itemVersion;
            attrib.GameVersion = gameVersion;
        }

        return attribs.empty() ? false : true;
    }

    return false;
}
//---------------------------------------------------------------------------
bool d2ce::ItemType::getRuneMagicalAttributes(const d2ce::Item& parentItem, std::vector<MagicalAttribute>& attribs) const
{
    attribs.clear();
    std::uint8_t parentGemApplyType = parentItem.getGemApplyType();
    if (!isRune())
    {
        return false;
    }

    auto iter = s_ItemGemsType.find(code);
    if (iter == s_ItemGemsType.end())
    {
        // should not happen
        return false;
    }
    switch (parentGemApplyType)
    {
    case 0:
        attribs = iter->second.weaponAttribs;
        break;

    case 1:
        attribs = iter->second.helmAttribs;
        break;

    case 2:
        attribs = iter->second.shieldAttribs;
        break;

    default:
        return false;
    }

    auto itemVersion = parentItem.getVersion();
    auto gameVersion = parentItem.getGameVersion();
    for (auto& attrib : attribs)
    {
        attrib.Version = itemVersion;
        attrib.GameVersion = gameVersion;
    }

    return attribs.empty() ? false : true;
}
//---------------------------------------------------------------------------
std::string d2ce::ItemType::getPotionDesc(d2ce::EnumCharClass charClass) const
{
    if (!isPotion())
    {
        return "";
    }

    std::stringstream ss;
    auto desc = spellDesc.desc;
    auto value = spellDesc.descCalc;
    switch (spellDesc.descFunc)
    {
    case 1:
        return spellDesc.desc;

    case 2:
    case 3:
    case 4:
        break;

    default:
        return "";
    }
    if (isManaPotion())
    {
        static const std::map <EnumCharClass, std::vector<std::uint16_t>> manaPointsMultDiv = {
            {EnumCharClass::Amazon, {3, 2}},      // add 1/2 of original value
            {EnumCharClass::Sorceress, {2, 1}},   // double original value
            {EnumCharClass::Necromancer, {2, 1}}, // double original value
            {EnumCharClass::Paladin, {3, 2}},     // add 1/2 of original value
            {EnumCharClass::Barbarian, {1, 1}},   // use original value
            {EnumCharClass::Druid, {2, 1}},       // double original value
            {EnumCharClass::Assassin, {3, 2}}     // add 1/2 of original value
        };

        auto iter = manaPointsMultDiv.find(charClass);
        if (iter != manaPointsMultDiv.end())
        {
            if (iter->second.size() >= 2)
            {
                value = (value * iter->second[0]) / iter->second[1];
            }
            else if (iter->second.empty())
            {
                value *= iter->second[0];
            }
        }

        if (desc.find("%") != desc.npos)
        {
            desc = LocalizationHelpers::string_format(desc, value);
        }
        else
        {
            // support classic txt files
            if (!desc.empty())
            {
                ss << desc;
                ss << " ";
            }
            ss << value;
            desc = ss.str();
        }
    }
    else if (isHealingPotion())
    {
        static const std::map <EnumCharClass, std::vector<std::uint16_t>> healingPointsAdd = {
            {EnumCharClass::Amazon, {3, 2}},      // add 1/2 of original value
            {EnumCharClass::Sorceress, {1, 1}},   // use original value
            {EnumCharClass::Necromancer, {1, 1}}, // use original value
            {EnumCharClass::Paladin, {3, 2}},     // add 1/2 of original value
            {EnumCharClass::Barbarian, {2, 1}},   // double original value
            {EnumCharClass::Druid, {1, 1}},       // use original value
            {EnumCharClass::Assassin, {3, 2}},    // add 1/2 of original value
        };

        auto iter = healingPointsAdd.find(charClass);
        if (iter != healingPointsAdd.end())
        {
            if (iter->second.size() >= 2)
            {
                value = (value * iter->second[0]) / iter->second[1];
            }
            else if (iter->second.empty())
            {
                value *= iter->second[0];
            }
        }

        if (desc.find("%") != desc.npos)
        {
            desc = LocalizationHelpers::string_format(desc, value);
        }
        else
        {
            // support classic txt files
            if (!desc.empty())
            {
                ss << desc;
                ss << " ";
            }
            ss << value;
            desc = ss.str();
        }
    }
    else if (isRejuvenationPotion())
    {
        if (desc.find("%") != desc.npos)
        {
            value = 100;
            if (isUpgradableRejuvenationPotion())
            {
                value = 35;
            }

            desc = LocalizationHelpers::string_format(desc, value);
        }
    }
    else
    {
        if (desc.find("%") != desc.npos)
        {
            desc = LocalizationHelpers::string_format(desc, value);
        }
        else
        {
            // support classic txt files
            if (!spellDesc.desc.empty())
            {
                ss << spellDesc.desc;
                ss << " ";
            }
            ss << value;
            desc = ss.str();
        }
    }

    return desc;
}
//---------------------------------------------------------------------------
const std::string& d2ce::ItemType::getRuneLetter() const
{
    static std::string badValue;
    if (!isRune())
    {
        return badValue;
    }

    auto iter = s_ItemGemsType.find(code);
    if (iter == s_ItemGemsType.end())
    {
        // should not happen
        return badValue;
    }

    return iter->second.letter;
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void d2ce::ItemHelpers::setTxtReader(const d2ce::ITxtReader& txtReader)
{
    if (s_pTextReader != &txtReader)
    {
        s_pTextReader = &txtReader;
        InitItemData(txtReader);
    }
}
//---------------------------------------------------------------------------
const d2ce::ITxtReader& d2ce::ItemHelpers::getTxtReader()
{
    if (s_pTextReader == nullptr)
    {
        return getDefaultTxtReader();
    }

    return *s_pTextReader;
}
//---------------------------------------------------------------------------
bool d2ce::ItemHelpers::isTxtReaderInitialized()
{
    return s_pTextReader == nullptr ? false : true;
}
//---------------------------------------------------------------------------
const std::string& d2ce::ItemHelpers::getLanguage()
{
    return s_CurrentLanguage.empty()? s_DefaultLanguage : s_CurrentLanguage;
}
//---------------------------------------------------------------------------
// work around bug in the debug compiler causing warning over range iterator
#pragma warning( push )
#pragma warning(disable : 4702)
const std::string& d2ce::ItemHelpers::setLanguage(const std::string& lang)
{
    if (lang.empty())
    {
        s_CurrentLanguage.clear(); // use default
    }
    else if (s_SupportedLanguages.find(lang) != s_SupportedLanguages.end())
    {
        s_CurrentLanguage = lang;
    }
    else if (lang.size() >= 3)
    {
        // look for the close match
        std::string match = lang.substr(0, 3);
        for (const auto& supportedLang : s_SupportedLanguages)
        {
            if (supportedLang.find(match) == 0)
            {
                s_CurrentLanguage = supportedLang;
            }
            break;
        }
    }

    return getLanguage();
}
#pragma warning( pop )
//---------------------------------------------------------------------------
const d2ce::ItemStat& d2ce::ItemHelpers::getItemStat(EnumItemVersion itemVersion, size_t idx)
{
    EnumCharVersion fileVersion = (itemVersion < EnumItemVersion::v109) ? EnumCharVersion::v100 : EnumCharVersion::v109;
    static d2ce::ItemStat badItemStat = { MAXUINT16 };
    auto itemStatsMapIter = s_ItemStatsInfo.find(fileVersion);
    if (itemStatsMapIter == s_ItemStatsInfo.end())
    {
        // should not happend
        itemStatsMapIter = s_ItemStatsInfo.begin();
        if (itemStatsMapIter == s_ItemStatsInfo.end())
        {
            return badItemStat;
        }
    }

    auto itemStatsIdIter = itemStatsMapIter->second.find((std::uint16_t)idx);
    if (itemStatsIdIter == itemStatsMapIter->second.end())
    {
        // should not happend
        return badItemStat;
    }

    return itemStatsIdIter->second;
}
//---------------------------------------------------------------------------
const d2ce::ItemStat& d2ce::ItemHelpers::getItemStat(EnumItemVersion itemVersion, const std::string& name)
{
    auto iter = s_ItemStatsNameMap.find(name);
    if (iter == s_ItemStatsNameMap.end())
    {
        static d2ce::ItemStat badItemStat = { MAXUINT16 };
        return badItemStat;
    }

    return ItemHelpers::getItemStat(itemVersion, iter->second);
}
//---------------------------------------------------------------------------
const d2ce::ItemStat& d2ce::ItemHelpers::getItemStat(const MagicalAttribute& attrib)
{
    return ItemHelpers::getItemStat(attrib.Version, attrib.Id);
}
//---------------------------------------------------------------------------
const bool d2ce::ItemHelpers::hasItemStat(EnumItemVersion itemVersion, size_t idx)
{
    EnumCharVersion fileVersion = (itemVersion < EnumItemVersion::v109) ? EnumCharVersion::v100 : EnumCharVersion::v109;
    static d2ce::ItemStat badItemStat = { MAXUINT16 };
    auto itemStatsMapIter = s_ItemStatsInfo.find(fileVersion);
    if (itemStatsMapIter == s_ItemStatsInfo.end())
    {
        // should not happend
        itemStatsMapIter = s_ItemStatsInfo.begin();
        if (itemStatsMapIter == s_ItemStatsInfo.end())
        {
            return false;
        }
    }

    auto itemStatsIdIter = itemStatsMapIter->second.find((std::uint16_t)idx);
    return (itemStatsIdIter == itemStatsMapIter->second.end()) ? false : true;
}
//---------------------------------------------------------------------------
const bool d2ce::ItemHelpers::hasItemStat(EnumItemVersion itemVersion, const std::string& name)
{
    auto iter = s_ItemStatsNameMap.find(name);
    if (iter == s_ItemStatsNameMap.end())
    {
        return false;
    }

    return ItemHelpers::hasItemStat(itemVersion, iter->second);
}
//---------------------------------------------------------------------------
const bool d2ce::ItemHelpers::hasItemStat(const MagicalAttribute& attrib)
{
    return ItemHelpers::hasItemStat(attrib.Version, attrib.Id);
}
//---------------------------------------------------------------------------
bool d2ce::ItemHelpers::getItemCodev100(std::uint16_t code, std::array<std::uint8_t, 4>& strcode)
{
    auto iter = s_ItemTypeCodes_v100.find(code);
    if (iter != s_ItemTypeCodes_v100.end())
    {
        for (size_t i = 0; i < 3; ++i)
        {
            strcode[i] = iter->second[i];
        }
        strcode[3] = 0x20;
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------
std::uint8_t d2ce::ItemHelpers::getItemCodev115(const std::vector<std::uint8_t>& data, size_t startOffset, std::array<std::uint8_t, 4>& strcode)
{
    size_t offset = startOffset;
    for (size_t i = 0; i < 4; ++i)
    {
        strcode[i] = GetEncodedChar(data, offset);
    }

    return std::uint8_t(offset - startOffset);
}
//---------------------------------------------------------------------------
// Retrieves encoded ItemCode (return number of bits set)
void d2ce::ItemHelpers::encodeItemCodev115(const std::array<std::uint8_t, 4>& strcode, std::uint64_t& encodedVal, std::uint8_t& numBitsSet)
{
    encodedVal = 0;
    numBitsSet = 0;
    static const std::map<std::uint8_t, std::vector<std::uint16_t> > huffmanEncodeMap = {
        {'0', {223, 8}}, { '1', { 31, 7}}, {'2', { 12, 6}}, {'3', { 91, 7}},
        {'4', { 95, 8}}, { '5', {104, 8}}, {'6', {123, 7}}, {'7', { 30, 5}},
        {'8', {  8, 6}}, { '9', { 14, 5}}, {' ', {  1, 2}}, {'a', { 15, 5}},
        {'b', { 10, 4}}, { 'c', {  2, 5}}, {'d', { 35, 6}}, {'e', {  3, 6}},
        {'f', { 50, 6}}, { 'g', { 11, 5}}, {'h', { 24, 5}}, {'i', { 63, 7}},
        {'j', {232, 9}}, { 'k', { 18, 6}}, {'l', { 23, 5}}, {'m', { 22, 5}},
        {'n', { 44, 6}}, { 'o', {127, 7}}, {'p', { 19, 5}}, {'q', {155, 8}},
        {'r', {  7, 5}}, { 's', {  4, 4}}, {'t', {  6, 5}}, {'u', { 16, 5}},
        {'v', { 59, 7}}, { 'w', {  0, 5}}, {'x', { 28, 5}}, {'y', { 40, 7}},
        {'z', { 27, 8}}, {'\0', {488, 9}},
    };

    for (std::uint8_t i = 4; i > 0; --i)
    {
        auto iter = huffmanEncodeMap.find(strcode[i - 1]);
        if (iter == huffmanEncodeMap.end())
        {
            // 0
            encodedVal <<= 9;
            encodedVal |= 488;
            numBitsSet += 9;
        }
        else
        {
            encodedVal <<= iter->second[1];
            encodedVal |= iter->second[0];
            numBitsSet += std::uint8_t(iter->second[1]);
        }
    }
}
//---------------------------------------------------------------------------
std::uint8_t d2ce::ItemHelpers::HuffmanDecodeBitString(const std::string& bitstr)
{
    auto iter = huffmanDecodeMap.find(bitstr);
    if (iter != huffmanDecodeMap.end())
    {
        return iter->second;
    }

    // something went wrong
    return UINT8_MAX;
}
//---------------------------------------------------------------------------
std::uint16_t d2ce::ItemHelpers::getGPSSortIndex(const std::array<std::uint8_t, 4>& strcode)
{
    auto &item = GetItemTypeHelper(strcode);

    std::uint16_t idx = 0;
    if (item.isGem())
    {
        if (item.hasCategoryCode("gema")) // amethyst
        {
            idx = 0;
        }
        else if (item.hasCategoryCode("gemd")) // diamond
        {
            idx = 1;
        }
        else if (item.hasCategoryCode("geme")) // emerald
        {
            idx = 2;
        }
        else if (item.hasCategoryCode("gemr")) // ruby
        {
            idx = 3;
        }
        else if (item.hasCategoryCode("gems")) // sapphire
        {
            idx = 4;
        }
        else if (item.hasCategoryCode("gemz")) // skull
        {
            idx = 5;
        }
        else if (item.hasCategoryCode("gemt")) // topaz
        {
            idx = 6;
        }
        else
        {
            return MAXUINT16; // unknown
        }

        if (item.hasCategoryCode("gem0")) // chipped
        {
            return idx;
        }

        if (item.hasCategoryCode("gem1")) // flawed
        {
            return idx + 7;
        }

        if (item.hasCategoryCode("gem2")) // regular
        {
            return idx + 14;
        }

        if (item.hasCategoryCode("gem3")) // flawless
        {
            return idx + 21;
        }

        if (item.hasCategoryCode("gem4")) // perfect
        {
            return idx + 28;
        }

        return MAXUINT16; // unknown
    }

    idx += 35;
    if (item.isPotion())
    {
        if (item.isHealingPotion()) // healing
        {
            const std::uint8_t& gemColour = strcode[2];
            switch (gemColour)
            {
            case '1': // Minor
                return idx;
            case '2': // Light
                return idx + 1;
            case '3': // Regular
                return idx + 2;
            case '4': // Greater
                return idx + 3;
            default: // Super
                return idx + 4;
            }
        }

        idx += 5;
        if (item.isManaPotion()) // mana
        {
            const std::uint8_t& gemColour = strcode[2];
            idx = 40;
            switch (gemColour)
            {
            case '1': // Minor
                return idx;
            case '2': // Light
                return idx + 1;
            case '3': // Regular
                return idx + 2;
            case '4': // Greater
                return idx + 3;
            default: // Super
                return idx + 4;
            }
        }

        idx += 5;
        if (item.isRejuvenationPotion()) // rejuvenation potion
        {
            return item.isUpgradableRejuvenationPotion() ? idx : idx + 1;
        }
        
        idx += 2;
        if (item.hasCategoryCode("apot")) // antidote potion
        {
            return idx;
        }

        ++idx;
        if (item.hasCategoryCode("spot")) //stamina potion
        {
            return idx;
        }

        ++idx;
        if (item.hasCategoryCode("wpot")) // thawing potion
        {
            return idx;
        }

        return MAXUINT16; // unknown
    }
    idx += 15;

    if (item.isRune())
    {
        const std::uint8_t& gemCondition = strcode[1];
        const std::uint8_t& gemColour = strcode[2];
        switch (gemCondition)
        {
        case '0': // El Rune - Ort Rune
            if (gemColour >= '1' && gemColour <= '9')
            {
                return idx + gemColour - '1';
            }
            break;

        case '1': // Thul Rune - Fal Rune
            idx += 9;
            if (gemColour >= '0' && gemColour <= '9')
            {
                return idx + gemColour - '0';
            }
            break;

        case '2': // Lem Rune - Sur Rune
            idx += 19;
            if (gemColour >= '0' && gemColour <= '9')
            {
                return idx + gemColour - '0';
            }
            break;

        case '3': // Ber Rune - Zod Rune
            idx += 29;
            if (gemColour >= '0' && gemColour <= '3')
            {
                return idx + gemColour - '0';
            }
            break;

        default:
            return MAXUINT16; // unknown

        }
    }

    return MAXUINT16; // unknown
}
//---------------------------------------------------------------------------
void d2ce::ItemHelpers::getValidGPSCodes(std::vector<std::string>& gpsCodes, bool isExpansion)
{
    gpsCodes.clear();
    gpsCodes.reserve(83);

    std::map<std::uint16_t, std::uint32_t> gemIdxMap;
    std::array<std::uint8_t, 4> gemCode = { 0x20, 0x20, 0x20, 0x20 };
    std::uint32_t& itemData = *reinterpret_cast<std::uint32_t*>(gemCode.data());
    std::string typeStr;
    std::uint16_t idx = 0;
    for (const auto& gpsItem : s_ItemMiscType)
    {
        typeStr = gpsItem.first.c_str();
        if (typeStr.length() < 3)
        {
            // invalid
            continue;
        }

        if (gpsItem.second.isUnusedItem())
        {
            continue;
        }

        if (!gpsItem.second.isPotion() && !gpsItem.second.isGem() && !gpsItem.second.isRune())
        {
            continue;
        }

        if(!isExpansion && gpsItem.second.isExpansionItem())
        {
            // Rune are only available in expansion
            continue;
        }

        gemCode[0] = typeStr[0];
        gemCode[1] = typeStr[1];
        gemCode[2] = typeStr[2];
        idx = getGPSSortIndex(gemCode);
        if (idx == MAXUINT16)
        {
            // invalid
            continue;
        }

        gemIdxMap.emplace(idx, itemData);
    }

    for (auto& gem : gemIdxMap)
    {
        *reinterpret_cast<std::uint32_t*>(gemCode.data()) = gem.second;
        gpsCodes.push_back(std::string(reinterpret_cast<char*>(gemCode.data()), 4));
    }
}
//---------------------------------------------------------------------------
const d2ce::ItemType& d2ce::ItemHelpers::getItemTypeHelper(const std::array<std::uint8_t, 4>& strcode)
{
    return GetItemTypeHelper(strcode);
}
//---------------------------------------------------------------------------
const d2ce::ItemType& d2ce::ItemHelpers::getItemTypeHelper(const std::string& code)
{
    return GetItemTypeHelper(code);
}
//---------------------------------------------------------------------------
const d2ce::ItemType& d2ce::ItemHelpers::getInvalidItemTypeHelper()
{
    return s_invalidItemType;
}
//---------------------------------------------------------------------------
void d2ce::ItemHelpers::initRunewordData()
{
    auto& txtReader = ItemHelpers::getTxtReader();
    InitRunewordData(txtReader);
}
//---------------------------------------------------------------------------
std::string d2ce::ItemHelpers::getRunewordNameFromId(std::uint16_t id)
{
    auto iter = s_ItemRunewordsType.find(id);
    if (iter != s_ItemRunewordsType.end())
    {
        return iter->second.name;
    }

    return "";
}
//---------------------------------------------------------------------------
const d2ce::RunewordType& d2ce::ItemHelpers::getRunewordFromId(std::uint16_t id)
{
    auto iter = s_ItemRunewordsType.find(id);
    if (iter != s_ItemRunewordsType.end())
    {
        return iter->second;
    }

    static d2ce::RunewordType badValue;
    return badValue;
}
//---------------------------------------------------------------------------
std::vector<d2ce::RunewordType> d2ce::ItemHelpers::getPossibleRunewords(const d2ce::Item& item, bool bUseCurrentSocketCount, bool bExcludeServerOnly)
{
    return getPossibleRunewords(item, 0ui32, bUseCurrentSocketCount, bExcludeServerOnly);
}
//---------------------------------------------------------------------------
std::vector<d2ce::RunewordType> d2ce::ItemHelpers::getPossibleRunewords(const d2ce::Item& item, std::uint32_t level, bool bUseCurrentSocketCount, bool bExcludeServerOnly)
{
    std::vector<d2ce::RunewordType> result;
    switch (item.getQuality())
    {
    case EnumItemQuality::MAGIC:
    case EnumItemQuality::SET:
    case EnumItemQuality::RARE:
    case EnumItemQuality::TEMPERED:
    case EnumItemQuality::CRAFT:
    case EnumItemQuality::UNIQUE:
        // runewords do not work with these
        return result;
    }

    auto itemVersion = item.getVersion();
    if (itemVersion < EnumItemVersion::v107)
    {
        return result;
    }

    auto numSockets = bUseCurrentSocketCount ? item.getDisplayedSocketCount() : std::max(item.getMaxSocketCount(), item.getDisplayedSocketCount());
    if (numSockets == 0)
    {
        return result;
    }

    auto minSockets = bUseCurrentSocketCount ? numSockets : item.getSocketCountBonus() + 1;

    std::vector<std::string> categories;
    item.getCategories(categories);

    bool isEquipped = item.getLocation() == d2ce::EnumItemLocation::EQUIPPED;

    // get possible runewords
    auto iterNumSockets = s_ItemNumRunesRunewordsMap.find(numSockets);
    while (numSockets >= minSockets)
    {
        iterNumSockets = s_ItemNumRunesRunewordsMap.find(numSockets);
        --numSockets;

        if (iterNumSockets == s_ItemNumRunesRunewordsMap.end())
        {
            continue;
        }

        auto iter = s_ItemRunewordsType.end();
        const auto& runwordIds = iterNumSockets->second;
        for (const auto& runewordId : runwordIds)
        {
            iter = s_ItemRunewordsType.find(runewordId);
            if (iter != s_ItemRunewordsType.end())
            {
                const auto& runeword = iter->second;
                if (bExcludeServerOnly && runeword.serverOnly)
                {
                    // skip
                    continue;
                }

                if (isEquipped && (level > 0) && (runeword.levelreq > level))
                {
                    // skip
                    continue;
                }

                if (itemVersion < runeword.version)
                {
                    // skip
                    continue;
                }

                for (const auto& exclude : runeword.excluded_categories)
                {
                    if (std::find(categories.begin(), categories.end(), exclude) != categories.end())
                    {
                        continue;
                    }
                }

                bool bIncluded = false;
                for (const auto& include : runeword.included_categories)
                {
                    if (std::find(categories.begin(), categories.end(), include) != categories.end())
                    {
                        bIncluded = true;
                        break;
                    }
                }

                if (!bIncluded)
                {
                    continue;
                }

                result.push_back(runeword);
            }
        }
    }

    return result;
}
//---------------------------------------------------------------------------
bool d2ce::ItemHelpers::getSetMagicAttribs(std::uint16_t id, std::vector<MagicalAttribute>& attribs, EnumItemVersion version, std::uint16_t gameVersion, std::uint16_t level, std::uint32_t dwb)
{
    attribs.clear();
    auto iter = s_ItemSetItemsType.find(id);
    if (iter == s_ItemSetItemsType.end())
    {
        return false;
    }

    auto& setitem = iter->second;
    if (setitem.version > gameVersion)
    {
        return false;
    }

    std::array<std::uint8_t, 4> strcode = { 0x20, 0x20, 0x20, 0x20 };
    size_t strcodeSize = std::min(setitem.code.size(), size_t(3));
    for (size_t i = 0; i < strcodeSize; ++i)
    {
        strcode[i] = setitem.code[i];
    }

    const auto& itemType = getItemTypeHelper(strcode);
    if (&itemType == &s_invalidItemType)
    {
        return false;
    }

    if (dwb == 0)
    {
        dwb = setitem.dwbCode;
    }

    ItemRandStruct rnd = { dwb, 666 };
    InitalizeItemRandomization(dwb, level, EnumItemQuality::MAGIC, rnd);
    ProcessMagicalProperites(setitem.modType, attribs, rnd, version, gameVersion);
    return true;
}
//---------------------------------------------------------------------------
bool d2ce::ItemHelpers::getUniqueMagicAttribs(std::uint16_t id, std::vector<MagicalAttribute>& attribs, EnumItemVersion version, std::uint16_t gameVersion, std::uint16_t level, std::uint32_t dwb)
{
    attribs.clear();
    auto iter = s_ItemUniqueItemsType.find(id);
    if(iter == s_ItemUniqueItemsType.end())
    {
        return false;
    }

    auto& uniqueitem = iter->second;
    if (uniqueitem.version > gameVersion)
    {
        return false;
    }

    std::array<std::uint8_t, 4> strcode = { 0x20, 0x20, 0x20, 0x20 };
    size_t strcodeSize = std::min(uniqueitem.code.size(), size_t(3));
    for (size_t i = 0; i < strcodeSize; ++i)
    {
        strcode[i] = uniqueitem.code[i];
    }

    if (dwb == 0)
    {
        dwb = generarateRandomDW();
    }

    ItemRandStruct rnd = { dwb, 666 };
    InitalizeItemRandomization(dwb, level, EnumItemQuality::MAGIC, rnd);
    ProcessMagicalProperites(uniqueitem.modType, attribs, rnd, version, gameVersion);
    return true;
}
//---------------------------------------------------------------------------
bool d2ce::ItemHelpers::getUniqueQuestMagicAttribs(const std::array<std::uint8_t, 4>& strcode, std::vector<MagicalAttribute>& attribs, EnumItemVersion version, std::uint16_t gameVersion, std::uint16_t level, std::uint32_t dwb)
{
    attribs.clear();

    std::string testStr("   ");
    testStr[0] = (char)strcode[0];
    testStr[1] = (char)strcode[1];
    testStr[2] = (char)strcode[2];
    auto iter = s_ItemUniqueQuestItemsIndex.find(testStr);
    if (iter == s_ItemUniqueQuestItemsIndex.end())
    {
        return false;
    }

    return getUniqueMagicAttribs(iter->second, attribs, version, gameVersion, level, dwb);
}
//---------------------------------------------------------------------------
std::uint16_t d2ce::ItemHelpers::getSetItemId(std::uint16_t id, const std::array<std::uint8_t, 4>& strcode)
{
    auto iterSets = s_ItemSetsType.find(id);
    if (iterSets == s_ItemSetsType.end())
    {
        return MAXUINT16;
    }

    // find set item
    std::string testStr("   ");
    testStr[0] = (char)strcode[0];
    testStr[1] = (char)strcode[1];
    testStr[2] = (char)strcode[2];
    for (const auto& setItem : iterSets->second.items)
    {
        auto iter = s_ItemSetItemsIndex.find(setItem);
        if (iter == s_ItemSetItemsIndex.end())
        {
            // should not happen
            continue;
        }

        const auto& setItemType = s_ItemSetItemsType[iter->second];
        if (setItemType.code == testStr)
        {
            return setItemType.id;
        }
    }

    return MAXUINT16;
}
//---------------------------------------------------------------------------
std::uint32_t d2ce::ItemHelpers::getSetItemDWBCode(std::uint16_t setItemId)
{
    auto iter = s_ItemSetItemsType.find(setItemId);
    if (iter == s_ItemSetItemsType.end())
    {
        return false;
    }

    return iter->second.dwbCode;
}
//---------------------------------------------------------------------------
std::uint32_t d2ce::ItemHelpers::getSetItemDWBCode(std::uint16_t id, const std::array<std::uint8_t, 4>& strcode)
{
    const auto& setItem = GetSetItemType(id, strcode);
    if (setItem.code.empty())
    {
        // invalid
        return 0;
    }

    return setItem.dwbCode;
}
//---------------------------------------------------------------------------
std::uint8_t d2ce::ItemHelpers::generateInferiorQualityId(std::uint16_t level, std::uint32_t dwb)
{
    if (dwb == 0)
    {
        dwb = generarateRandomDW();
    }

    ItemRandStruct rnd = { dwb, 666 };
    InitalizeItemRandomization(dwb, level, EnumItemQuality::INFERIOR, rnd);
    return std::uint8_t(GenerateRandom(rnd) % 4);
}
//---------------------------------------------------------------------------
bool d2ce::ItemHelpers::generateMagicalAffixes(const std::array<std::uint8_t, 4>& strcode, MagicalCachev100& cache, EnumItemVersion version, std::uint16_t gameVersion, std::uint16_t level, std::uint32_t dwb)
{
    std::vector<ItemAffixType> prefixes;
    std::vector<ItemAffixType> suffixes;
    if (!GenerateMagicalAffixesBuffer(strcode, prefixes, suffixes, gameVersion, level))
    {
        return false;
    }

    if (dwb == 0)
    {
        dwb = generarateRandomDW();
    }

    ItemRandStruct rnd = { dwb, 666 };
    InitalizeItemRandomization(dwb, level, EnumItemQuality::MAGIC, rnd);

    // Do we have a prefix?
    cache.Affixes.PrefixId = 0;
    if ((GenerateRandom(rnd) % 2) == 1)
    {
        if (prefixes.empty())
        {
            return false;
        }

        auto& prefix = prefixes[GenerateRandom(rnd) % prefixes.size()];
        cache.Affixes.PrefixId = prefix.code;
        cache.Affixes.PrefixName = prefix.name;
        ProcessMagicalProperites(prefix.modType, cache.MagicalAttributes, rnd, version, gameVersion);
    }

    // Do we have a suffix
    cache.Affixes.SuffixId = 0;
    if ((GenerateRandom(rnd) % 2) == 1 || (cache.Affixes.PrefixId == 0))
    {
        if (suffixes.empty())
        {
            return false;
        }

        auto& suffix = suffixes[GenerateRandom(rnd) % suffixes.size()];
        cache.Affixes.SuffixId = suffix.code;
        cache.Affixes.SuffixName = suffix.name;
        ProcessMagicalProperites(suffix.modType, cache.MagicalAttributes, rnd, version, gameVersion);
    }

    return true;
}
//---------------------------------------------------------------------------
bool d2ce::ItemHelpers::generateRareOrCraftedAffixes(const std::array<std::uint8_t, 4>& strcode, RareOrCraftedCachev100& cache, EnumItemVersion version, std::uint16_t gameVersion, std::uint16_t level, std::uint32_t dwb)
{
    const auto& itemType = getItemTypeHelper(strcode);
    if (&itemType == &s_invalidItemType)
    {
        return false;
    }

    std::vector<ItemAffixType> prefixes;
    std::vector<ItemAffixType> suffixes;
    if (!GenerateMagicalAffixesBuffer(strcode, prefixes, suffixes, gameVersion, level))
    {
        return false;
    }

    if (prefixes.empty() || suffixes.empty())
    {
        return false;
    }

    std::vector<ItemAffixType> rarePrefixes;
    std::vector<ItemAffixType> rareSuffixes;
    if (!GenerateRareOrCraftedAffixesBuffer(strcode, rarePrefixes, rareSuffixes, gameVersion))
    {
        return false;
    }

    if (rarePrefixes.empty() || rareSuffixes.empty())
    {
        return false;
    }

    if (dwb == 0)
    {
        dwb = generarateRandomDW();
    }

    ItemRandStruct rnd = { dwb, 666 };
    InitalizeItemRandomization(dwb, level, EnumItemQuality::MAGIC, rnd);

    const auto& affix1 = rarePrefixes[GenerateRandom(rnd) % rarePrefixes.size()];
    cache.Id = affix1.code;
    cache.Name = affix1.name;
    cache.Index = affix1.index;

    const auto& affix2 = rareSuffixes[GenerateRandom(rnd) % rareSuffixes.size()];
    cache.Id2 = affix2.code;
    cache.Name2 = affix2.name;
    cache.Index2 = affix2.name;

    std::uint32_t nTotalPreSuffixes = (GenerateRandom(rnd) % 3) + 4;
    std::uint32_t nPrefixes = 0;
    std::uint32_t nSuffixes = 0;
    std::uint32_t infiniteGuard = 1000000; // no more loops then this
    std::vector<ItemAffixType> curPreSuffix;
    std::vector<bool> isPrefix;
    for (size_t nCurPreSuffix = 0; nCurPreSuffix < nTotalPreSuffixes; ++nCurPreSuffix)
    {
        // Do we have a prefix?
        auto PreSuf = GenerateRandom(rnd) % 2;
        GenerateRandom(rnd); // skip the next random number
        if ((PreSuf == 0 && nPrefixes < 3) || (nSuffixes > 2))
        {
            bool bDone = false;
            std::uint32_t idx = 0;
            while (!bDone)
            {
                if (infiniteGuard == 0)
                {
                    return false;
                }
                --infiniteGuard;

                idx = GenerateRandom(rnd) % prefixes.size();
                bDone = true;
                for (auto iter = curPreSuffix.rbegin(); iter != curPreSuffix.rend(); ++iter)
                {
                    if (iter->group != prefixes[idx].group)
                    {
                        GenerateRandom(rnd); // skip the next random number
                        bDone = false;
                        break;
                    }
                }
            }

            curPreSuffix.push_back(prefixes[idx]);
            isPrefix.push_back(true);
            ++nPrefixes;
        }
        else
        {
            bool bDone = false;
            std::uint32_t idx = 0;
            while (!bDone)
            {
                if (infiniteGuard == 0)
                {
                    return false;
                }
                --infiniteGuard;

                idx = GenerateRandom(rnd) % suffixes.size();
                bDone = true;
                for (auto iter = curPreSuffix.rbegin(); iter != curPreSuffix.rend(); ++iter)
                {
                    if (iter->group != suffixes[idx].group)
                    {
                        GenerateRandom(rnd); // skip the next random number
                        bDone = false;
                        break;
                    }
                }
            }

            curPreSuffix.push_back(suffixes[idx]);
            isPrefix.push_back(false);
            ++nSuffixes;
        }
    }

    cache.affixes.resize(std::max(nPrefixes, nSuffixes));
    nPrefixes = 0;
    nSuffixes = 0;
    auto iterIsPrefix = isPrefix.begin();
    for (auto iter = curPreSuffix.begin(); iter != curPreSuffix.end() && iterIsPrefix != isPrefix.end(); ++iter, ++iterIsPrefix)
    {
        if (*iterIsPrefix)
        {
            auto& affixItem = cache.affixes[nPrefixes];
            ++nPrefixes;

            affixItem.Affixes.PrefixId = iter->code;
            affixItem.Affixes.PrefixName = iter->name;
            if (affixItem.Affixes.SuffixId == MAXUINT16)
            {
                affixItem.Affixes.SuffixId = 0;
            }

            ProcessMagicalProperites(iter->modType, affixItem.MagicalAttributes, rnd, version, gameVersion);
        }
        else
        {
            auto& affixItem = cache.affixes[nSuffixes];
            ++nSuffixes;

            affixItem.Affixes.SuffixId = iter->code;
            affixItem.Affixes.SuffixName = iter->name;
            if (affixItem.Affixes.PrefixId == MAXUINT16)
            {
                affixItem.Affixes.PrefixId = 0;
            }

            ProcessMagicalProperites(iter->modType, affixItem.MagicalAttributes, rnd, version, gameVersion);
        }
    }

    return true;
}
//---------------------------------------------------------------------------
std::uint16_t d2ce::ItemHelpers::generateDefenseRating(const std::array<std::uint8_t, 4>& strcode, std::uint32_t dwa)
{
    if (dwa == 0)
    {
        dwa = generarateRandomDW();
    }

    const auto& itemType = getItemTypeHelper(strcode);
    if (&itemType == &s_invalidItemType)
    {
        return 0;
    }

    if (itemType.ac.Max > itemType.ac.Min)
    {
        return std::uint16_t(generateDWARandomOffset(dwa, 2) % (itemType.ac.Max - itemType.ac.Min)) + itemType.ac.Min;
    }

    return itemType.ac.Min;
}
//---------------------------------------------------------------------------
std::uint32_t d2ce::ItemHelpers::generateDWARandomOffset(std::uint32_t dwa, std::uint16_t numRndCalls)
{
    ItemRandStruct rnd = { dwa, 666 };
    for (size_t i = 0; i < numRndCalls; ++i)
    {
        GenerateRandom(rnd);
    }

    return rnd.seed;
}
//---------------------------------------------------------------------------
std::uint32_t d2ce::ItemHelpers::generarateRandomDW()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<std::uint32_t> spread(0, MAXUINT32);
    return std::uint32_t(spread(gen) + (spread(gen) << 16));
}
//---------------------------------------------------------------------------
std::string d2ce::ItemHelpers::getSetNameFromId(std::uint16_t id)
{
    auto iter = s_ItemSetItemsType.find(id);
    if (iter == s_ItemSetItemsType.end())
    {
        return "";
    }

    return iter->second.name;
}
//---------------------------------------------------------------------------
std::string d2ce::ItemHelpers::getSetTCFromId(std::uint16_t id)
{
    auto iter = s_ItemSetItemsType.find(id);
    if (iter == s_ItemSetItemsType.end())
    {
        return "";
    }

    return iter->second.transform_color;
}
//---------------------------------------------------------------------------
std::uint16_t d2ce::ItemHelpers::getSetLevelReqFromId(std::uint16_t id)
{
    auto iter = s_ItemSetItemsType.find(id);
    if (iter == s_ItemSetItemsType.end())
    {
        return 0;
    }

    return iter->second.level.levelreq;
}
//---------------------------------------------------------------------------
const std::string& d2ce::ItemHelpers::getRareNameFromId(std::uint16_t id)
{
    const auto& affix = getRareAffixType(id);
    return affix.name;
}
//---------------------------------------------------------------------------
const std::string& d2ce::ItemHelpers::getRareIndexFromId(std::uint16_t id)
{
    const auto& affix = getRareAffixType(id);
    return affix.index;
}
//---------------------------------------------------------------------------
const std::string& d2ce::ItemHelpers::getMagicalPrefixFromId(std::uint16_t id)
{
    auto iter = s_ItemMagicPrefixType.find(id);
    if (iter != s_ItemMagicPrefixType.end())
    {
        return iter->second.name;
    }

    static std::string badValue;
    return badValue;
}
//---------------------------------------------------------------------------
const std::string& d2ce::ItemHelpers::getMagicalSuffixFromId(std::uint16_t id)
{
    auto iter = s_ItemMagicSuffixType.find(id);
    if (iter != s_ItemMagicSuffixType.end())
    {
        return iter->second.name;
    }

    static std::string badValue;
    return badValue;
}
//---------------------------------------------------------------------------
const std::string& d2ce::ItemHelpers::getMagicalPrefixTCFromId(std::uint16_t id)
{
    auto iter = s_ItemMagicPrefixType.find(id);
    if (iter != s_ItemMagicPrefixType.end())
    {
        return iter->second.transform_color;
    }

    static std::string badValue;
    return badValue;
}
//---------------------------------------------------------------------------
const std::string& d2ce::ItemHelpers::getMagicalSuffixTCFromId(std::uint16_t id)
{
    auto iter = s_ItemMagicSuffixType.find(id);
    if (iter != s_ItemMagicSuffixType.end())
    {
        return iter->second.transform_color;
    }

    static std::string badValue;
    return badValue;
}
//---------------------------------------------------------------------------
const std::string& d2ce::ItemHelpers::getUniqueNameFromId(std::uint16_t id)
{
    static std::string badValue;
    auto iter = s_ItemUniqueItemsType.find(id);
    return (iter == s_ItemUniqueItemsType.end()) ? badValue : iter->second.name;
}
//---------------------------------------------------------------------------
std::uint16_t d2ce::ItemHelpers::getIdFromRareIndex(const std::string& rareIndex)
{
    auto iter = s_ItemRareIndex.find(rareIndex);
    return (iter == s_ItemRareIndex.end()) ? 0ui16 : iter->second;
}
//---------------------------------------------------------------------------
std::uint16_t d2ce::ItemHelpers::getIdFromRareName(const std::string& rareName)
{
    auto iter = s_ItemRareNames.find(rareName);
    return (iter == s_ItemRareNames.end()) ? 0ui16 : iter->second;
}
//---------------------------------------------------------------------------
std::uint16_t d2ce::ItemHelpers::getUniqueLevelReqFromId(std::uint16_t id)
{
    auto iter = s_ItemUniqueItemsType.find(id);
    return (iter == s_ItemUniqueItemsType.end()) ? 0ui16 : iter->second.level.levelreq;
}
//---------------------------------------------------------------------------
std::string d2ce::ItemHelpers::getUniqueTCFromId(std::uint16_t id)
{
    auto iter = s_ItemUniqueItemsType.find(id);
    return (iter == s_ItemUniqueItemsType.end()) ? "" : iter->second.transform_color;
}
//---------------------------------------------------------------------------
std::uint16_t d2ce::ItemHelpers::getMagicalPrefixLevelReqFromId(std::uint16_t id)
{
    auto iter = s_ItemMagicPrefixType.find(id);
    if (iter != s_ItemMagicPrefixType.end())
    {
        return iter->second.level.levelreq;
    }

    return 0;
}
//---------------------------------------------------------------------------
std::uint16_t d2ce::ItemHelpers::getMagicalSuffixLevelReqFromId(std::uint16_t id)
{
    auto iter = s_ItemMagicSuffixType.find(id);
    if (iter != s_ItemMagicSuffixType.end())
    {
        return iter->second.level.levelreq;
    }

    return 0;
}
//---------------------------------------------------------------------------
bool d2ce::ItemHelpers::magicalAttributeSorter(const MagicalAttribute& left, const MagicalAttribute& right)
{
    if (left.DescPriority == 0)
    {
        if (right.DescPriority == 0)
        {
            return (left.Id < right.Id);
        }

        return false;
    }

    if (right.DescPriority == 0)
    {
        return true;
    }

    if (left.DescPriority == right.DescPriority)
    {
        if (left.Id == right.Id)
        {
            if (left.Values.empty())
            {
                return right.Values.empty();
            }

            if (right.Values.empty())
            {
                return true;
            }

            return left.Values[0] > right.Values[0];
        }

        return (left.Id < right.Id);
    }

    return (left.DescPriority > right.DescPriority);
}
//---------------------------------------------------------------------------
void d2ce::ItemHelpers::checkForRelatedMagicalAttributes(std::vector<MagicalAttribute>& attribs)
{
    if (attribs.empty())
    {
        return;
    }

    // check for the "all" cases
    auto itemVersion = attribs.front().Version;
    std::uint16_t strengthId = getItemStat(itemVersion, "strength").id;
    std::uint16_t energyId = getItemStat(itemVersion, "energy").id;
    std::uint16_t dexterityId = getItemStat(itemVersion, "dexterity").id;
    std::uint16_t vitalityId = getItemStat(itemVersion, "vitality").id;
    std::uint16_t fireresistId = getItemStat(itemVersion, "fireresist").id;
    std::uint16_t lightresistId = getItemStat(itemVersion, "lightresist").id;
    std::uint16_t coldresistId = getItemStat(itemVersion, "coldresist").id;
    std::uint16_t poisonresistId = getItemStat(itemVersion, "poisonresist").id;
    std::uint16_t mindamageId = getItemStat(itemVersion, "mindamage").id;
    std::uint16_t maxdamageId = getItemStat(itemVersion, "maxdamage").id;
    std::uint16_t secondary_mindamageId = getItemStat(itemVersion, "secondary_mindamage").id;
    std::uint16_t secondary_maxdamageId = getItemStat(itemVersion, "secondary_maxdamage").id;
    std::uint16_t item_throw_mindamageId = getItemStat(itemVersion, "item_throw_mindamage").id;
    std::uint16_t item_throw_maxdamageId = getItemStat(itemVersion, "item_throw_maxdamage").id;

    std::map<std::uint16_t, std::reference_wrapper<MagicalAttribute>> relatedPropIdMap;
    std::int64_t numRelatedProps[4] = { 0, 0, 0, 0 };
    std::int64_t relatedPropValues[4] = { 0, 0, 0, 9 };
    bool validProps[4] = { true, true, true, true };
    for (auto& attrib : attribs)
    {
        if (!validProps[0] && !validProps[1])
        {
            return;
        }

        if ((attrib.Id == strengthId) || (attrib.Id == energyId) || (attrib.Id == dexterityId) || (attrib.Id == vitalityId))
        {
            if (validProps[0])
            {
                if (relatedPropIdMap.find(attrib.Id) == relatedPropIdMap.end())
                {
                    relatedPropIdMap.emplace(attrib.Id, std::ref(attrib));
                    if ((numRelatedProps[0] == 0) || (attrib.Values[0] == relatedPropValues[0]))
                    {
                        ++numRelatedProps[0];
                        relatedPropValues[0] = attrib.Values[0];
                    }
                    else
                    {
                        // non matching case
                        validProps[0] = false;
                        numRelatedProps[0] = 0;
                    }
                }
                else
                {
                    // more than one poperty of the same type
                    validProps[0] = false;
                    numRelatedProps[0] = 0;
                }
            }
        }
        else if ((attrib.Id == fireresistId) || (attrib.Id == lightresistId) || (attrib.Id == coldresistId) || (attrib.Id == poisonresistId))
        {
            if (validProps[1])
            {
                if (relatedPropIdMap.find(attrib.Id) == relatedPropIdMap.end())
                {
                    relatedPropIdMap.emplace(attrib.Id, std::ref(attrib));
                    if ((numRelatedProps[1] == 0) || (attrib.Values[0] == relatedPropValues[1]))
                    {
                        ++numRelatedProps[1];
                        relatedPropValues[1] = attrib.Values[0];
                    }
                    else
                    {
                        // non matching case
                        validProps[1] = false;
                        numRelatedProps[1] = 0;
                    }
                }
                else
                {
                    // more than one poperty of the same type
                    validProps[1] = false;
                    numRelatedProps[1] = 0;
                }
            }
        }
        else if ((attrib.Id == mindamageId) || (attrib.Id == secondary_mindamageId) || (attrib.Id == item_throw_mindamageId))
        {
            if (validProps[2])
            {
                if (relatedPropIdMap.find(attrib.Id) == relatedPropIdMap.end())
                {
                    relatedPropIdMap.emplace(attrib.Id, std::ref(attrib));
                    if ((numRelatedProps[2] == 0) || (attrib.Values[0] == relatedPropValues[2]))
                    {
                        ++numRelatedProps[2];
                        relatedPropValues[2] = attrib.Values[0];
                    }
                    else
                    {
                        // non matching case
                        validProps[2] = false;
                        numRelatedProps[2] = 0;
                    }
                }
                else
                {
                    // more than one poperty of the same type
                    validProps[2] = false;
                    numRelatedProps[2] = 0;
                }
            }
        }
        else if ((attrib.Id == maxdamageId) || (attrib.Id == secondary_maxdamageId) || (attrib.Id == item_throw_maxdamageId))
        {
            if (validProps[3])
            {
                if (relatedPropIdMap.find(attrib.Id) == relatedPropIdMap.end())
                {
                    relatedPropIdMap.emplace(attrib.Id, std::ref(attrib));
                    if ((numRelatedProps[3] == 0) || (attrib.Values[0] == relatedPropValues[3]))
                    {
                        ++numRelatedProps[3];
                        relatedPropValues[3] = attrib.Values[0];
                    }
                    else
                    {
                        // non matching case
                        validProps[3] = false;
                        numRelatedProps[3] = 0;
                    }
                }
                else
                {
                    // more than one poperty of the same type
                    validProps[3] = false;
                    numRelatedProps[3] = 0;
                }
            }
        }
    }

    for (size_t i = 0; i < 2; ++i)
    {
        if (validProps[i] && (numRelatedProps[i] != 4))
        {
            validProps[i] = false;
        }
    }

    for (size_t i = 2; i < 4; ++i)
    {
        if (validProps[i] && (numRelatedProps[i] == 1))
        {
            validProps[i] = false;
        }
    }

    if (!validProps[0] && !validProps[1] && !validProps[2] && !validProps[3])
    {
        return;
    }

    bool minDamageVisible = true;
    bool maxDamageVisible = true;
    for (auto& prop : relatedPropIdMap)
    {
        auto& attrib = prop.second.get();
        if ((attrib.Id == strengthId) || (attrib.Id == energyId) || (attrib.Id == dexterityId) || (attrib.Id == vitalityId))
        {
            if (validProps[0])
            {
                const auto& stat = getItemStat(attrib.Version, strengthId);
                attrib.Desc = stat.descGrp;
                attrib.Visible = (attrib.Id == vitalityId) ? true : false;
            }

        }
        else if ((attrib.Id == fireresistId) || (attrib.Id == lightresistId) || (attrib.Id == coldresistId) || (attrib.Id == poisonresistId))
        {
            if (validProps[1])
            {
                const auto& stat = getItemStat(attrib.Version, fireresistId);
                attrib.Desc = stat.descGrp;
                attrib.Visible = (attrib.Id == poisonresistId) ? true : false;
            }
        }
        else if ((attrib.Id == mindamageId) || (attrib.Id == secondary_mindamageId) || (attrib.Id == item_throw_mindamageId))
        {
            if (validProps[2])
            {
                if (!minDamageVisible)
                {
                    attrib.Visible = false;
                }
                minDamageVisible = false;
            }
        }
        else if ((attrib.Id == maxdamageId) || (attrib.Id == secondary_maxdamageId) || (attrib.Id == item_throw_maxdamageId))
        {
            if (validProps[2])
            {
                if (!maxDamageVisible)
                {
                    attrib.Visible = false;
                }
                maxDamageVisible = false;
            }
        }
    }
}
//---------------------------------------------------------------------------
std::int64_t d2ce::ItemHelpers::getMagicalAttributeValue(MagicalAttribute& attrib, std::uint32_t charLevel, size_t idx, const ItemStat& stat)
{
    if (idx >= attrib.Values.size())
    {
        return 0;
    }

    auto value = attrib.Values[idx];
    std::stringstream ssValue;
    auto nextInChain = stat.nextInChain;
    auto descFunc = stat.descFunc;
    auto* pOpAttribs = &stat.opAttribs;
    if (stat.nextInChain != 0)
    {
        if (stat.name.compare("poisonmindam") == 0)
        {
            switch (idx)
            {
            case 0:
            case 1:
                if (attrib.Values.size() >= 3)
                {
                    return (value * attrib.Values[2] + 128) / 256;
                }
                return value;

                // value at index 3 is 25 * duration of postion damage
            case 2:
                return value / 25;

            default:
                return value;
            }
        }

        // group stat, get the stat information we are looking for
        while ((idx > 0) && (nextInChain != 0))
        {
            --idx;
            const auto& stat2 = getItemStat(attrib.Version, nextInChain);
            nextInChain = stat2.nextInChain;
            descFunc = stat2.descFunc;
            pOpAttribs = &stat2.opAttribs;
        }
    }

    switch (descFunc)
    {
    case  5: 
    case 10:
        switch (idx)
        {
        case 0: // value at index 0 with range 1 to 128 must be converted to a percentage
            return  (value * 100 + 64) / 128;

        default:
            return value;
        }
        break;

    case 11:
        switch (idx)
        {
        case 0: // value at index 0 is durability units per 100 seconds
            return 100 / value;

        default:
            return value;
        }
        break;
    }

    if ((idx == 0) && (pOpAttribs != nullptr))
    {
        // stats based on character level
        switch (pOpAttribs->op)
        {
        case  1:
        case 11:
        case 13:
            if (pOpAttribs->op_base == "level")
            {
                attrib.OpValue = (charLevel * attrib.Values[0]) / 100;
                attrib.OpStats = pOpAttribs->op_stats;
                return attrib.OpValue;
            }
            break;

        case  2:
        case  3:
        case  4:
        case  5:
            if (pOpAttribs->op_base == "level")
            {
                attrib.OpValue = (charLevel * attrib.Values[0]) / std::int64_t(std::pow(2, std::int64_t(pOpAttribs->op_param)));
                attrib.OpStats = pOpAttribs->op_stats;
                return attrib.OpValue;
            }
            break;
        }
    }

    return value;
}
//---------------------------------------------------------------------------
void d2ce::ItemHelpers::applyNonMaxMagicalAttributes(CharStats& cs, std::vector<MagicalAttribute>& attribs)
{
    for (auto& attrib : attribs)
    {
        switch (attrib.Id)
        {
        case  0: // strength
            cs.Strength += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
            continue;

        case  1: // energy
            cs.Energy += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
            continue;

        case  2: // dexterity
            cs.Dexterity += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
            continue;

        case  3: // vitality
            cs.Vitality += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
            continue;
        }

        const auto& stat = ItemHelpers::getItemStat(attrib);
        if (stat.opAttribs.op_base == "level")
        {
            for (const auto& opStat : stat.opAttribs.op_stats)
            {
                if (opStat == "strength")
                {
                    switch (stat.opAttribs.op)
                    {
                    case 2:
                        cs.Strength += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
                        break;
                    }
                }
                else if (opStat == "energy")
                {
                    switch (stat.opAttribs.op)
                    {
                    case 2:
                        cs.Energy += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
                        break;
                    }
                }
                else if (opStat == "dexterity")
                {
                    switch (stat.opAttribs.op)
                    {
                    case 2:
                        cs.Dexterity += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
                        break;
                    }
                }
                else if (opStat == "vitality")
                {
                    switch (stat.opAttribs.op)
                    {
                    case 2:
                        cs.Vitality += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
                        break;
                    }
                }
            }
        }
    }
}
//---------------------------------------------------------------------------
void d2ce::ItemHelpers::applyMaxMagicalAttributes(CharStats& cs, std::vector<MagicalAttribute>& attribs)
{
    for (auto& attrib : attribs)
    {
        switch (attrib.Id)
        {
        case  7: // maxhp
            cs.MaxLife += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
            continue;

        case  9: // maxmana
            cs.MaxMana += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
            continue;

        case 11: // maxstamina
            cs.MaxStamina += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
            continue;
        }

        const auto& stat = ItemHelpers::getItemStat(attrib);
        if (stat.opAttribs.op_base == "level")
        {
            for (const auto& opStat : stat.opAttribs.op_stats)
            {
                if (opStat == "maxhp")
                {
                    switch (stat.opAttribs.op)
                    {
                    case 2:
                        cs.MaxLife += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
                        break;
                    }
                }
                else if (opStat == "maxmana")
                {
                    switch (stat.opAttribs.op)
                    {
                    case 2:
                        cs.MaxMana += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
                        break;
                    }
                }
                else if (opStat == "maxstamina")
                {
                    switch (stat.opAttribs.op)
                    {
                    case 2:
                        cs.MaxStamina += std::uint32_t(Items::getMagicalAttributeValue(attrib, cs.Level, 0));
                        break;
                    }
                }
            }
        }
    }
}
//---------------------------------------------------------------------------
bool d2ce::ItemHelpers::formatMagicalAttributes(std::vector<MagicalAttribute>& attribs, std::uint32_t charLevel)
{
    // check for the "all" cases
    ItemHelpers::checkForRelatedMagicalAttributes(attribs);

    bool bFormatted = false;
    for (auto& attrib : attribs)
    {
        bFormatted |= ItemHelpers::formatDisplayedMagicalAttribute(attrib, charLevel);
    }

    // Sort display items in proper order
    std::sort(attribs.begin(), attribs.end(), ItemHelpers::magicalAttributeSorter);
    return bFormatted;
}
//---------------------------------------------------------------------------
std::string d2ce::ItemHelpers::formatMagicalAttributeValue(MagicalAttribute& attrib, std::uint32_t charLevel, size_t idx, const ItemStat& stat)
{
    std::string strSkill;
    std::string strClass;
    if (idx >= attrib.Values.size())
    {
        // special handling
        switch (stat.descFunc)
        {
        case 27: // +[value] to [skill] ([class] Only)
            if (attrib.Values.size() == 2 && idx == 2)
            {
                // ([class] Only) replacement
                auto value = getMagicalAttributeValue(attrib, charLevel, 0, stat);
                GetClassAndSkillStrings(value, strClass, strSkill);
                return strClass;
            }
            break;
        }

        return "";
    }

    auto value = getMagicalAttributeValue(attrib, charLevel, idx, stat);

    std::string strValue;
    switch (stat.descFunc)
    {
    case 13: // +[value] to [class] Skill Levels
        switch (idx)
        {
        case 0:
            return CharClassHelper::getStrAllSkills(std::uint16_t(value));

        default:
            return std::to_string(value);
        }
        break;

    case 14: // +[value] to [skilltab] Skill Levels ([class] Only)
        switch (idx)
        {
        case 0:
            if ((value < 3) && (attrib.Values.size() >= 2))
            {
                return CharClassHelper::getStrSkillTab(std::uint16_t(value), std::uint16_t(attrib.Values[1]));
            }

            return std::to_string(value);

        case 1:
            return CharClassHelper::getStrClassOnly(std::uint16_t(value));

        default:
            return std::to_string(value);
        }
        break;

    case 15: // [chance]% to cast [slvl] [skill] on [event]
    case 24: // charges
        switch (idx)
        {
        case 1: // skill name is index 1
            GetClassAndSkillStrings(value, strClass, strSkill);
            if (!strSkill.empty())
            {
                return strSkill;
            }

            return std::to_string(value);

        default:
            return std::to_string(value);
        }
        break;

    case 16: // Level [sLvl] [skill] Aura When Equipped
    case 28: // +[value] to [skill]
        switch (idx)
        {
        case 0: // skill name is index 0
            GetClassAndSkillStrings(value, strClass, strSkill);
            if (!strSkill.empty())
            {
                return strSkill;
            }

            return std::to_string(value);

        default:
            return std::to_string(value);
        }
        break;

    case 27: // +[value] to [skill] ([class] Only)
        switch (idx)
        {
        case 0: // Character specific Skill index is index 0
            GetClassAndSkillStrings(value, strClass, strSkill);
            if (strClass.empty())
            {
                if (strSkill.empty())
                {
                    strSkill = std::to_string(value);
                }

                return strSkill;
            }

            if (strSkill.empty())
            {
                strSkill = std::to_string(value);
            }

            return strSkill;

        default:
            return std::to_string(value);
        }
        break;
    }

    return std::to_string(value);
}
//---------------------------------------------------------------------------
bool d2ce::ItemHelpers::formatDisplayedMagicalAttribute(MagicalAttribute& attrib, std::uint32_t charLevel)
{
    if (attrib.Desc.empty() || attrib.Values.empty())
    {
        return false;
    }

    // Check special cases
    size_t repIdx = 0;
    std::string replaceStr;
    const auto& stat = getItemStat(attrib.Version, attrib.Id);
    if (stat.nextInChain != 0)
    {
        if (attrib.Values.size() >= 2 && attrib.Values[0] == attrib.Values[1])
        {
            attrib.Desc = stat.descNoRange;
        }
        else
        {
            attrib.Desc = stat.descRange;
        }
    }
    else if (attrib.Values.size() == 1 && attrib.Values[0] < 0)
    {
        attrib.Desc = stat.descNeg;
    }

    auto maxIdx = attrib.Values.size();
    switch (stat.descFunc)
    {
    case 27: // +[value] to [skill] ([class] Only)
        ++maxIdx;
        break;
    }

    for (size_t idx = 0; idx < maxIdx; ++idx)
    {
        std::stringstream ssReplace;
        ssReplace << "{";
        ssReplace << idx;
        ssReplace << "}";
        replaceStr = ssReplace.str();

        repIdx = attrib.Desc.find(replaceStr);
        if (repIdx == attrib.Desc.npos)
        {
            continue;
        }

        attrib.Desc.replace(repIdx, replaceStr.size(), formatMagicalAttributeValue(attrib, charLevel, idx, stat));
    }

    return true;
}
//---------------------------------------------------------------------------
void d2ce::ItemHelpers::combineMagicalAttribute(std::multimap<size_t, size_t>& itemIndexMap, const std::vector<MagicalAttribute>& newAttribs, std::vector<MagicalAttribute>& attribs)
{
    size_t numPoisonAttribs = 0;
    size_t numPoisonTimeSum = 0;
    size_t numColdAttribs = 0;
    size_t numColdTimeSum = 0;
    for (const auto& attrib : newAttribs)
    {
        auto iter = itemIndexMap.lower_bound(attrib.Id);
        auto iterEnd = itemIndexMap.upper_bound(attrib.Id);
        if (iter == iterEnd)
        {
            itemIndexMap.insert(std::make_pair(attrib.Id, attribs.size()));
            attribs.push_back(attrib);
            switch (attrib.Id)
            {
            case 54:
                numColdAttribs = 1;
                numColdTimeSum = attrib.Values[2];
                break;

            case 57:
                numPoisonAttribs = 1;
                numPoisonTimeSum = attrib.Values[2];
                break;
            }
        }
        else
        {
            bool notMatched = true;
            for (; notMatched && iter != iterEnd; ++iter)
            {
                auto& existing = attribs.at(iter->second);
                if (existing.Values.empty())
                {
                    continue;
                }

                // Check to see if we are a match to merge
                switch (attrib.Id)
                {
                case 17:
                case 48:
                case 50:
                case 52:
                    existing.Values[0] += attrib.Values[0];
                    existing.Values[1] += attrib.Values[1];
                    notMatched = false;
                    break;

                case 54:
                    ++numColdAttribs;
                    numColdTimeSum += existing.Values[2];
                    existing.Values[0] += attrib.Values[0];
                    existing.Values[1] += attrib.Values[1];
                    existing.Values[2] = numColdTimeSum / numColdAttribs; // average
                    notMatched = false;
                    break;

                case 57:
                    ++numPoisonAttribs;
                    numPoisonTimeSum += existing.Values[2];
                    existing.Values[0] += attrib.Values[0];
                    existing.Values[1] += attrib.Values[1];
                    existing.Values[2] = numPoisonTimeSum / numPoisonAttribs; // average
                    notMatched = false;
                    break;

                default:
                {
                    const auto& stat = getItemStat(attrib.Version, attrib.Id);
                    bool goodMatch = true;
                    size_t numMatchValue = (stat.encode == 3) ? 2 : 1;
                    for (size_t idx = 0; idx < existing.Values.size() - numMatchValue; ++idx)
                    {
                        if (existing.Values[idx] != attrib.Values[idx])
                        {
                            goodMatch = false;
                            break;
                        }
                    }

                    if (goodMatch)
                    {
                        notMatched = false;
                        for (std::int64_t idx = (std::int64_t)existing.Values.size() - 1; idx >= (std::int64_t)existing.Values.size() - (std::int64_t)numMatchValue; --idx)
                        {
                            existing.Values[idx] += attrib.Values[idx];
                        }
                    }
                    break;
                }
                }
            }

            if (notMatched)
            {
                itemIndexMap.insert(std::make_pair(attrib.Id, attribs.size()));
                attribs.push_back(attrib);
            }
        }
    }
}
//---------------------------------------------------------------------------
bool d2ce::ItemHelpers::ProcessNameNode(const Json::Value& node, std::array<char, NAME_LENGTH>& name)
{
    if (node.isNull())
    {
        return false;
    }

    {
        // Check Name
        // Remove any invalid characters from the name
        std::string curName(node.asString());
        LocalizationHelpers::CheckCharName(curName);
        name.fill(0);
        strcpy_s(name.data(), curName.length() + 1, curName.c_str());
        name[15] = 0; // must be zero
        return true;
    }
}

//---------------------------------------------------------------------------
namespace d2ce
{
    namespace LocalizationHelpers
    {
        bool GetLocalizedString(const std::map<size_t, std::map<std::string, std::string>>& stringTxtInfo, 
            size_t id, std::string& outStr, std::string& gender)
        {
            auto iter = stringTxtInfo.find(id);
            if (iter == stringTxtInfo.end())
            {
                return false;
            }

            const auto& lang = ItemHelpers::getLanguage();
            auto iterLang = iter->second.find(lang);
            if (iterLang != iter->second.end())
            {
                outStr = ExtractGenderString(iterLang->second, gender);
                return true;
            }

            // check for compatible language
            std::string matchStr = lang.substr(0, 2);
            for (const auto& langStr : iter->second)
            {
                if (langStr.first.find(matchStr) == 0)
                {
                    outStr = ExtractGenderString(langStr.second, gender);
                    return true;
                }
            }

            // Check for default language
            iterLang = iter->second.find(s_DefaultLanguage);
            if (iterLang != iter->second.end())
            {
                outStr = ExtractGenderString(iterLang->second, gender);
                return true;
            }

            // find any language
            iterLang = iter->second.begin();
            if (iterLang != iter->second.end())
            {
                outStr = ExtractGenderString(iterLang->second, gender);
                return true;
            }

            return false;
        }

        bool GetLocalizedString(const std::map<std::string, size_t>& stringTxtIdxInfo,
            const std::map<size_t, std::map<std::string, std::string>>& stringTxtInfo,
            const std::string str, std::string& outStr, std::string& gender)
        {
            auto iter = stringTxtIdxInfo.find(str);
            if (iter == stringTxtIdxInfo.end())
            {
                return false;
            }

            return GetLocalizedString(stringTxtInfo, iter->second, outStr, gender);
        }

        std::vector<std::string>& ProcessCharTitleList(const std::vector<std::string>& titleList, const std::vector<std::string>& defList, std::vector<std::string>& results)
        {
            if (results.size() < (titleList.size() + 1))
            {
                results.resize(titleList.size() + 1);
            }

            size_t idx = 0;
            static std::string placeholder("%s");
            std::string strValueDef;
            for (const auto& str : titleList)
            {
                strValueDef = defList[idx];
                ++idx;
                GetStringTxtValue(str, results[idx], strValueDef);
                auto pos = results[idx].find(placeholder);
                if (pos != std::string::npos)
                {
                    results[idx].erase(pos, placeholder.length());
                }
            }

            return results;
        }
    }
}

//---------------------------------------------------------------------------
bool d2ce::LocalizationHelpers::GetStringTxtValue(const std::string& str, std::string& outStr, std::string& gender, const char* defValue)
{
    if (defValue != nullptr)
    {
        outStr = defValue;
    }
    else
    {
        outStr.clear();
    }

    if (str.empty())
    {
        return false;
    }

    if (GetLocalizedString(s_PatchStringTxtIdxInfo, s_PatchStringTxtInfo, str, outStr, gender))
    {
        return true;
    }

    if (GetLocalizedString(s_ExpansionStringTxtIdxInfo, s_ExpansionStringTxtInfo, str, outStr, gender))
    {
        return true;
    }

    if (GetLocalizedString(s_StringTxtIdxInfo, s_StringTxtInfo, str, outStr, gender))
    {
        return true;
    }

    if (defValue == nullptr)
    {
        outStr = str;
    }
    return false;
}
//---------------------------------------------------------------------------
bool d2ce::LocalizationHelpers::GetStringTxtValue(const std::string& str, std::string& outStr, const char* defValue)
{
    std::string gender;
    return GetStringTxtValue(str, outStr, gender, defValue);
}
//---------------------------------------------------------------------------
bool d2ce::LocalizationHelpers::GetStringTxtValue(size_t id, std::string& outStr, std::string& gender, const char* defValue)
{
    if (defValue != nullptr)
    {
        outStr = defValue;
    }
    else
    {
        outStr.clear();
    }

    if (GetLocalizedString(s_PatchStringTxtInfo, id, outStr, gender))
    {
        return true;
    }

    if (GetLocalizedString(s_ExpansionStringTxtInfo, id, outStr, gender))
    {
        return true;
    }

    if (GetLocalizedString(s_StringTxtInfo, id, outStr, gender))
    {
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------
bool d2ce::LocalizationHelpers::GetStringTxtValue(size_t id, std::string& outStr, const char* defValue)
{
    std::string gender;
    return GetStringTxtValue(id, outStr, gender, defValue);
}
//---------------------------------------------------------------------------
const std::string& d2ce::LocalizationHelpers::GetIndestructibleStringTxtValue(std::string& outStr, std::string& gender)
{
    const std::string str("ModStre9s");
    GetStringTxtValue(str, outStr, gender, "Indestructible");
    return outStr;
}
//---------------------------------------------------------------------------
const std::string& d2ce::LocalizationHelpers::GetIndestructibleStringTxtValue(std::string& outStr)
{
    std::string gender;
    return GetIndestructibleStringTxtValue(outStr, gender);
}
//---------------------------------------------------------------------------
const std::string& d2ce::LocalizationHelpers::GetEtherealStringTxtValue(std::string& outStr, std::string& gender)
{
    const std::string str("strethereal");
    GetStringTxtValue(str, outStr, gender, "Ethereal (Cannot be Repaired)");
    return outStr;
}
//---------------------------------------------------------------------------
const std::string& d2ce::LocalizationHelpers::GetEtherealStringTxtValue(std::string& outStr)
{
    std::string gender;
    return GetEtherealStringTxtValue(outStr, gender);
}
//---------------------------------------------------------------------------
const std::string& d2ce::LocalizationHelpers::GetSocketedStringTxtValue(std::string& outStr, std::string& gender)
{
    const std::string str("Socketable");
    GetStringTxtValue(str, outStr, gender, "Socketed (%i)");
    return outStr;
}
//---------------------------------------------------------------------------
const std::string& d2ce::LocalizationHelpers::GetSocketedStringTxtValue(std::string& outStr)
{
    std::string gender;
    return GetSocketedStringTxtValue(outStr, gender);
}
//---------------------------------------------------------------------------
const std::string& d2ce::LocalizationHelpers::GetDifficultyStringTxtValue(EnumDifficulty diff, std::string& outStr)
{
    outStr.clear();
    switch (diff)
    {
    case EnumDifficulty::Normal:
        GetStringTxtValue("strCreateGameNormalText", outStr, "Normal");
        break;
    case EnumDifficulty::Nightmare:
        GetStringTxtValue("strCreateGameNightmareText", outStr, "Nightmare");
        break;
    case EnumDifficulty::Hell:
        GetStringTxtValue("strCreateGameHellText", outStr, "Hell");
        break;
    }
    return outStr;
}
const std::string& d2ce::LocalizationHelpers::CheckCharName(std::string& curName, bool bASCII)
{
    auto uCurName = utf8::utf8to32(curName);
    std::u32string uNewText;
    for (size_t iPos = 0, numberOfUnderscores = 0, nLen = uCurName.size(); iPos < nLen; ++iPos)
    {
        auto c = uCurName[iPos];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-' ||
            (!bASCII && (c == 0x8A || c == 0x8C || c == 0x8E
                || c == 0x0A || c == 0x9C || c == 0x9E || c == 0x9F
                || (c >= 0xC0 && c <= 0xD0) || (c >= 0xD8 && c <= 0xF6) || c >= 0xF8)))
        {
            uNewText += c;
        }
        else if ((c == '_' || c == '-') && !uNewText.empty() && numberOfUnderscores < 1)
        {
            uNewText += c;
            ++numberOfUnderscores;
        }
    }

    // trim bad characters
    if (uNewText.size() > 15)
    {
        uNewText.resize(15);
    }

    // trim bad characters
    uNewText.erase(uNewText.find_last_not_of(U"_-") + 1);
    if (uNewText.size() >= 2)
    {
        curName = utf8::utf32to8(uNewText);
    }
    else
    {
        uNewText = uCurName;
    }

    while (curName.size() > 16)
    {
        uNewText.pop_back();
        uNewText.erase(uNewText.find_last_not_of(U"_-") + 1);
        curName = utf8::utf32to8(uNewText);
    }

    return curName;
}

//---------------------------------------------------------------------------
const std::vector<std::string> d2ce::LocalizationHelpers::GetCharacterTitles(bool isFemale, bool isHardcore, bool isExpansion)
{
    std::string placeholder(" %s");
    std::string str;
    std::string strValueDef;
    std::vector<std::string> results(NUM_OF_TITLES);
    if (isHardcore)
    {
        if (isExpansion)
        {
            if (isFemale)
            {
                static std::vector<std::string> all_hardcore_expansion_female_default = { "Destroyer %s", "Conqueror %s", "Guardian %s" };
                static std::vector<std::string> all_hardcore_expansion_female = { "FemaleDestroyerPlayerTitle", "FemaleConquerorPlayerTitle", "FemaleGuardianPlayerTitle" };
                return ProcessCharTitleList(all_hardcore_expansion_female, all_hardcore_expansion_female, results);
            }

            static std::vector<std::string> all_hardcore_expansion_male_default = { "Destroyer %s", "Conqueror %s", "Guardian %s" };
            static std::vector<std::string> all_hardcore_expansion_male = { "MaleDestroyerPlayerTitle", "MaleConquerorPlayerTitle", "MaleGuardianPlayerTitle" };
            return ProcessCharTitleList(all_hardcore_expansion_male, all_hardcore_expansion_male_default, results);
        }

        if (isFemale)
        {
            static std::vector<std::string> all_hardcore_female_default = { "Countess %s", "Duchess %s", "Queen %s" };
            static std::vector<std::string> all_hardcore_female = { "CountessPlayerTitle", "DuchessPlayerTitle", "QueenPlayerTitle" };
            return ProcessCharTitleList(all_hardcore_female, all_hardcore_female_default, results);
        }

        static std::vector<std::string> all_hardcore_male_default = { "Count %s", "Duke %s", "King %s" };
        static std::vector<std::string> all_hardcore_male = { "CountPlayerTitle", "DukePlayerTitle", "KingPlayerTitle" };
        return ProcessCharTitleList(all_hardcore_male, all_hardcore_male, results);
    }

    if (isExpansion)
    {
        if (isFemale)
        {
            static std::vector<std::string> all_expansion_female_default = { "Slayer %s", "Champion %s", "Matriarch %s" };
            static std::vector<std::string> all_expansion_female = { "FemaleSlayerPlayerTitle", "FemaleChampionPlayerTitle", "MatriarchPlayerTitle" };
            return ProcessCharTitleList(all_expansion_female, all_expansion_female_default, results);
        }

        static std::vector<std::string> all_expansion_male_default = { "Slayer %s", "Champion %s", "Patriarch %s" };
        static std::vector<std::string> all_expansion_male = { "MaleSlayerPlayerTitle", "MaleChampionPlayerTitle", "PatriarchPlayerTitle" };
        return ProcessCharTitleList(all_expansion_male, all_expansion_male_default, results);
    }

    if (isFemale)
    {
        static std::vector<std::string> all_female_default = { "Dame %s", "Lady %s", "Baroness %s" };
        static std::vector<std::string> all_female = { "DamePlayerTitle", "LadyPlayerTitle", "BaronessPlayerTitle" };
        return ProcessCharTitleList(all_female, all_female_default, results);
    }

    static std::vector<std::string> all_male_default = { "Sir %s", "Lord %s", "Baron %s" };
    static std::vector<std::string> all_male = { "SirPlayerTitle", "LordPlayerTitle", "BaronPlayerTitle" };
    return ProcessCharTitleList(all_male, all_male_default, results);
}
//---------------------------------------------------------------------------
const std::vector<std::string> d2ce::LocalizationHelpers::GetCharacterTypes(bool isExpansion)
{
    std::vector<std::string> results;
    results.reserve(7);
    static std::vector<std::string> all_legacy_types = { "Amazon", "Sorceress", "Necromancer", "Paladin", "Barbarian" };
    for (const auto& str : all_legacy_types)
    {
        results.resize(results.size() + 1);
        GetStringTxtValue(str, results.back());
    }

    if (isExpansion)
    {
        static std::vector<std::string> all_expansion_types = { "Druid", "Assassin" };
        for (const auto& str : all_expansion_types)
        {
            results.resize(results.size() + 1);
            GetStringTxtValue(str, results.back());
        }
    }

    return results;
}
//---------------------------------------------------------------------------
