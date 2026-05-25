// ============================================================
//  QuestTrader | QuestData.c  (v1.5)
//  Fixed: all array members marked as ref
// ============================================================

enum QT_QuestType
{
    COLLECT = 0,
    KILL    = 1,
    DELIVER = 2,
    INTERACT = 3
}

enum QT_QuestState
{
    AVAILABLE   = 0,
    ACTIVE      = 1,
    COMPLETED   = 2,
    TURNED_IN   = 3,
    COOLDOWN    = 4
}

// Toast type enum lives here in 4_World so server can use it
enum QT_ToastType
{
    INFO     = 0,
    ACCEPT   = 1,
    PROGRESS = 2,
    COMPLETE = 3,
    REWARD   = 4,
    WARNING  = 5
}

class QT_Objective
{
    string itemClassName;
    string entityClassName;
    ref array<string> Targets;
    // Campo para alvo nomeado específico (ex: nome de NPC customizado).
    // Quando preenchido, APENAS entidades cujo GetDisplayName() ou
    // variável m_qtTargetName contenha esse valor contarão para a quest.
    // Pode ser combinado com entityClassName/Targets para restringir
    // ainda mais: classe correta E nome correto.
    string specificTargetName;
    int    requiredAmount;
    string description;
    int    currentAmount;

    void QT_Objective()
    {
        itemClassName      = "";
        entityClassName    = "";
        Targets            = new array<string>();
        specificTargetName = "";
        requiredAmount     = 1;
        description        = "";
        currentAmount      = 0;
    }

    bool IsComplete() { return currentAmount >= requiredAmount; }
    int  Remaining()  { return Math.Max(0, requiredAmount - currentAmount); }
}

class QT_Reward
{
    string itemClassName;
    int    amount;

    void QT_Reward() { itemClassName = ""; amount = 1; }
}

class QT_SpawnItem
{
    string itemClass;
    vector position;
}

class QT_QuestDef
{
    string              id;
    string              traderId;
    string              title;
    string              description;
    int                 type; // 0=COLLECT 1=KILL 2=DELIVER 3=INTERACT
    bool                repeatable;
    int                 cooldownHours;
    ref array<ref QT_Objective>  objectives;
    ref array<string>   Targets;           // Optional KILL shortcut: creates one objective when objectives is empty
    int                 RequiredAmount;    // Optional KILL shortcut used with Targets
    ref array<ref QT_Reward>     rewards;
    string              rewardMessage;
    string              acceptMessage;
    string              journalStory;       // Optional: story text shown in the player's completed quest journal
    ref array<string>   prerequisiteQuestIds;
    string              deliveryTraderId;   // DELIVER: trader to deliver to
    string              deliveryItemClass;  // DELIVER: item given on accept
    string              spawnItemClass;     // Optional: item spawned at world position on accept
    vector              spawnPosition;      // World position for spawnItemClass
    ref array<ref QT_SpawnItem> spawnItems; // Optional: multiple items to spawn on accept
    bool                spawnKillTarget;    // KILL: spawn configured targets only when true
    vector              killTargetSpawnPosition;
    float               killTargetSpawnRadius;
    string              interactionObjectClassName; // INTERACT: class/name fragment to match
    vector              interactionPosition;        // INTERACT: fixed map position to use
    float               interactionDistance;
    bool                spawnInteractionObject;     // INTERACT: optional editor-created object spawn

    void QT_QuestDef()
    {
        objectives           = new array<ref QT_Objective>();
        Targets              = new array<string>();
        RequiredAmount       = 0;
        rewards              = new array<ref QT_Reward>();
        prerequisiteQuestIds = new array<string>();
        deliveryTraderId     = "";
        deliveryItemClass    = "";
        spawnItemClass       = "";
        journalStory         = "";
        spawnPosition        = vector.Zero;
        spawnItems           = new array<ref QT_SpawnItem>();
        spawnKillTarget      = false;
        killTargetSpawnPosition = vector.Zero;
        killTargetSpawnRadius = 20.0;
        interactionObjectClassName = "";
        interactionPosition  = vector.Zero;
        interactionDistance  = 3.0;
        spawnInteractionObject = false;
    }
}

class QT_PlayerQuestState
{
    string           questId;
    int              state;
    ref array<int>   objectiveProgress;
    int              completedTimestamp;

    void QT_PlayerQuestState()
    {
        state                = 0; // QT_QuestState.AVAILABLE
        objectiveProgress    = new array<int>();
        completedTimestamp   = 0;
    }
}

class QT_TraderDef
{
    string  id;
    string  name;
    vector  position;
    vector  orientation;
    string  model;
    string  greeting;
    string  farewell;
    ref array<string> greetings;
    ref array<string> outfit;  // classnames to equip on spawn

    void QT_TraderDef()
    {
        greetings = new array<string>();
        position    = Vector(0,0,0);
        orientation = Vector(0,0,0);
        outfit      = new array<string>();
    }
}

class QT_Settings
{
    float   interactionDistance;
    bool    showQuestMarkersOnMap;
    bool    notifyOnKillProgress;
    bool    EnableKillDebug;
    bool    debugLogging;
    bool    ActivateSucessSound;
    bool    EnableQuestChatMessages;
    bool    EnableKillTargetSpawning;   // Global toggle - set false to disable all kill quest animal/creature spawning
    bool    EnableQuestTraderPerformanceLogs;
    float   QuestTraderEventDebounceSeconds;
    float   QuestTraderSaveDebounceSeconds;
    int     QuestTraderMaxRpcItemsPerPacket;
    float   QuestSaveDebounceSeconds;
    int     QuestSaveMaxPlayersPerTick;
    float   QuestFullBackupIntervalSeconds;

    void QT_Settings()
    {
        interactionDistance   = 3.5;
        showQuestMarkersOnMap = true;
        notifyOnKillProgress  = true;
        EnableKillDebug       = false;
        debugLogging          = false;
        ActivateSucessSound   = true;
        EnableQuestChatMessages = true;
        EnableKillTargetSpawning = true;
        EnableQuestTraderPerformanceLogs = false;
        QuestTraderEventDebounceSeconds = 3.0;
        QuestTraderSaveDebounceSeconds = 10.0;
        QuestTraderMaxRpcItemsPerPacket = 50;
        QuestSaveDebounceSeconds = 15.0;
        QuestSaveMaxPlayersPerTick = 2;
        QuestFullBackupIntervalSeconds = 900.0;
    }
}

class QT_Config
{
    ref array<string>             AdminSteamIds;
    ref array<ref QT_TraderDef>  TraderNPCPositions;
    ref array<ref QT_QuestDef>   Quests;
    ref QT_Settings              Settings;

    void QT_Config()
    {
        AdminSteamIds     = new array<string>();
        TraderNPCPositions = new array<ref QT_TraderDef>();
        Quests             = new array<ref QT_QuestDef>();
        Settings           = new QT_Settings();
    }
}
