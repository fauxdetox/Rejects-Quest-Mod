// ============================================================
//  QuestTrader | QuestData.c  (v1.5)
//  Fixed: all array members marked as ref
// ============================================================

enum QT_QuestType
{
    COLLECT = 0,
    KILL    = 1,
    DELIVER = 2
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
    int    requiredAmount;
    string description;
    int    currentAmount;

    void QT_Objective()
    {
        itemClassName   = "";
        entityClassName = "";
        requiredAmount  = 1;
        description     = "";
        currentAmount   = 0;
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
    int                 type; // 0=COLLECT 1=KILL 2=DELIVER
    bool                repeatable;
    int                 cooldownHours;
    ref array<ref QT_Objective>  objectives;
    ref array<ref QT_Reward>     rewards;
    string              rewardMessage;
    string              acceptMessage;
    ref array<string>   prerequisiteQuestIds;
    string              deliveryTraderId;   // DELIVER: trader to deliver to
    string              deliveryItemClass;  // DELIVER: item given on accept
    string              spawnItemClass;     // Optional: item spawned at world position on accept
    vector              spawnPosition;      // World position for spawnItemClass
    ref array<ref QT_SpawnItem> spawnItems; // Optional: multiple items to spawn on accept

    void QT_QuestDef()
    {
        objectives           = new array<ref QT_Objective>();
        rewards              = new array<ref QT_Reward>();
        prerequisiteQuestIds = new array<string>();
        deliveryTraderId     = "";
        deliveryItemClass    = "";
        spawnItemClass       = "";
        spawnPosition        = vector.Zero;
        spawnItems           = new array<ref QT_SpawnItem>();
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
    bool    debugLogging;

    void QT_Settings()
    {
        interactionDistance   = 3.5;
        showQuestMarkersOnMap = true;
        notifyOnKillProgress  = true;
        debugLogging          = false;
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
