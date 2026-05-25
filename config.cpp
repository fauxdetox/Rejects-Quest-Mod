class CfgPatches
{
    class QuestTrader
    {
        units[]          = {};
        weapons[]        = {};
        requiredVersion  = 0.1;
        requiredAddons[] = { "DZ_Data", "DZ_Scripts", "DZ_Sounds_Effects" };
    };
};

class CfgSoundShaders
{
    class QuestTrader_Complete_SoundShader
    {
        samples[] = {{"QuestTrader\fx\complete", 1}};
        volume = 1;
        range = 8;
    };

    class QuestTrader_Ready_SoundShader
    {
        samples[] = {{"QuestTrader\fx\ready", 1}};
        volume = 1;
        range = 8;
    };

    class QuestTrader_JournalOpen_SoundShader
    {
        samples[] = {{"QuestTrader\fx\jornal_open", 1}};
        volume = 1;
        range = 8;
    };

    class QuestTrader_JournalPage_SoundShader
    {
        samples[] = {{"QuestTrader\fx\next_pages", 1}};
        volume = 1;
        range = 8;
    };

    class QuestTrader_JournalClose_SoundShader
    {
        samples[] = {{"QuestTrader\fx\book_close", 1}};
        volume = 1;
        range = 8;
    };
};

class CfgSoundSets
{
    class QuestTrader_Complete_SoundSet
    {
        soundShaders[] = {"QuestTrader_Complete_SoundShader"};
        volumeFactor = 1;
        frequencyFactor = 1;
        spatial = 1;
        doppler = 0;
        loop = 0;
        sound3DProcessingType = "character3DProcessingType";
        distanceFilter = "BaseCharacter_AttenuationFilter";
        volumeCurve = "characterAttenuationCurve";
    };

    class QuestTrader_Ready_SoundSet
    {
        soundShaders[] = {"QuestTrader_Ready_SoundShader"};
        volumeFactor = 1;
        frequencyFactor = 1;
        spatial = 1;
        doppler = 0;
        loop = 0;
        sound3DProcessingType = "character3DProcessingType";
        distanceFilter = "BaseCharacter_AttenuationFilter";
        volumeCurve = "characterAttenuationCurve";
    };

    class QuestTrader_JournalOpen_SoundSet
    {
        soundShaders[] = {"QuestTrader_JournalOpen_SoundShader"};
        volumeFactor = 1;
        frequencyFactor = 1;
        spatial = 1;
        doppler = 0;
        loop = 0;
        sound3DProcessingType = "character3DProcessingType";
        distanceFilter = "BaseCharacter_AttenuationFilter";
        volumeCurve = "characterAttenuationCurve";
    };

    class QuestTrader_JournalPage_SoundSet
    {
        soundShaders[] = {"QuestTrader_JournalPage_SoundShader"};
        volumeFactor = 1;
        frequencyFactor = 1;
        spatial = 1;
        doppler = 0;
        loop = 0;
        sound3DProcessingType = "character3DProcessingType";
        distanceFilter = "BaseCharacter_AttenuationFilter";
        volumeCurve = "characterAttenuationCurve";
    };

    class QuestTrader_JournalClose_SoundSet
    {
        soundShaders[] = {"QuestTrader_JournalClose_SoundShader"};
        volumeFactor = 1;
        frequencyFactor = 1;
        spatial = 1;
        doppler = 0;
        loop = 0;
        sound3DProcessingType = "character3DProcessingType";
        distanceFilter = "BaseCharacter_AttenuationFilter";
        volumeCurve = "characterAttenuationCurve";
    };
};

class CfgMods
{
    class QuestTrader
    {
        dir         = "QuestTrader";
        picture     = "";
        action      = "";
        hideName    = 0;
        hidePicture = 0;
        name        = "Quest Trader";
        credits     = "JhonWinchester";
        author      = "Taco Donkey";
        authorID    = "0";
        version     = "2.1.0";
        inputs      = "QuestTrader/inputs.xml";
        extra       = 0;
        type        = "mod";

        dependencies[] = { "Game", "World", "Mission" };

        class defs
        {
            class gameScriptModule
            {
                value  = "";
                files[] = { "QuestTrader/scripts/3_Game" };
            };
            class worldScriptModule
            {
                value  = "";
                files[] = { "QuestTrader/scripts/4_World" };
            };
            class missionScriptModule
            {
                value  = "";
                files[] = { "QuestTrader/scripts/5_Mission" };
            };
        };
    };
};
