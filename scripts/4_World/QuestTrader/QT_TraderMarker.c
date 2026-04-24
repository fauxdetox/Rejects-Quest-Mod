class QT_TraderMarker {

    private static ref QT_TraderMarker s_instance;
    
    static QT_TraderMarker GetInstance() {
        if (!s_instance)
            s_instance = new QT_TraderMarker();
        return s_instance;
    }

    #ifdef BXD_SystemParty
        ref array<ref BXDServerMarker> qt_SPMarkers = new array<ref BXDServerMarker>();
    #endif

    #ifdef LBmaster_Groups
        ref array<ref LBServerMarker> qt_LBMarkers = new array<ref LBServerMarker>();
    #endif

    #ifdef EXPANSIONMODNAVIGATION
        ref ExpansionMarkerModule m_QT_TraderMarkerEvent;
        ref array<ref ExpansionMarkerData> qt_ExpMarkers = new array<ref ExpansionMarkerData>();
    #endif

    #ifdef BASICMAP
        ref array<ref BasicMapMarker> qt_BasicMapMarkers = new array<ref BasicMapMarker>();
    #endif

    void QT_TraderMarker() {
        #ifdef EXPANSIONMODNAVIGATION
            CF_Modules<ExpansionMarkerModule>.Get(m_QT_TraderMarkerEvent);
        #endif
    }

    void CreateMarker(QT_TraderDef def) {
        if (!QT_QuestManager.GetInstance().GetConfig().Settings.showQuestMarkersOnMap) {
            return;
        }
        
        #ifdef BXD_SystemParty
            BXDServerMarker spMarker = BXDStaticMarkerManager.Get().AddTempServerMarker(def.name, def.position, "BXD_SystemParty\\gui\\icons\\player.paa", ARGB(255, 0, 255, 0));
            qt_SPMarkers.Insert(spMarker);
        #endif

        #ifdef LBmaster_Groups
            #ifdef LBmaster_Rework
                LBServerMarker lbMarker = LBStaticMarkerManager.Get.AddTempServerMarker(def.name, def.position, "LBmaster_Groups\\gui\\icons\\player.paa", ARGB(255, 0, 255, 0), false, true, true, true);
                qt_LBMarkers.Insert(lbMarker);
            #endif
            #ifndef LBmaster_Rework
                LBServerMarker lbMarker = LBStaticMarkerManager.Get().AddTempServerMarker(def.name, def.position, "LBmaster_Groups\\gui\\icons\\player.paa", ARGB(255, 0, 255, 0), false, true, true, true);
                qt_LBMarkers.Insert(lbMarker);
            #endif
        #endif

        #ifdef EXPANSIONMODNAVIGATION
            ExpansionMarkerData expMarker = m_QT_TraderMarkerEvent.CreateServerMarker(def.name, "Territory", def.position, ARGB(255, 0, 255, 0), true);
            qt_ExpMarkers.Insert(expMarker);
        #endif

        #ifdef BASICMAP
            // Calculate correct ground height for the marker position
            vector bmPos = def.position;
            bmPos[1] = GetGame().SurfaceY(bmPos[0], bmPos[2]);
            array<int> colour = {0, 220, 80};
            BasicMapMarker bmMarker = new BasicMapMarker(def.name, bmPos, "BasicMap\\gui\\images\\shop.paa", colour, 235, true);
            bmMarker.SetCanEdit(false);
            bmMarker.SetGroup(BasicMap().SERVER_KEY);
            BasicMap().AddMarker(BasicMap().SERVER_KEY, bmMarker);
            qt_BasicMapMarkers.Insert(bmMarker);
        #endif
    }
    
    void DeleteMarker() {
        #ifdef BXD_SystemParty
            foreach (BXDServerMarker spMarker : qt_SPMarkers) {
                if (spMarker) BXDStaticMarkerManager.Get().RemoveServerMarker(spMarker);
            }
            qt_SPMarkers.Clear();
        #endif

        #ifdef LBmaster_Groups
            #ifdef LBmaster_Rework
                foreach (LBServerMarker lbMarker : qt_LBMarkers) {
                    if (lbMarker) LBStaticMarkerManager.Get.RemoveServerMarker(lbMarker);
                }
                qt_LBMarkers.Clear();
            #endif
            #ifndef LBmaster_Rework
                foreach (LBServerMarker lbMarker : qt_LBMarkers) {
                    if (lbMarker) LBStaticMarkerManager.Get().RemoveServerMarker(lbMarker);
                }
                qt_LBMarkers.Clear();
            #endif
        #endif

        #ifdef EXPANSIONMODNAVIGATION
            if (m_QT_TraderMarkerEvent) {
                foreach (ExpansionMarkerData expMarker : qt_ExpMarkers) {
                    if (expMarker) m_QT_TraderMarkerEvent.RemoveServerMarker(expMarker.GetUID());
                }
            }
            qt_ExpMarkers.Clear();
        #endif

        #ifdef BASICMAP
            foreach (BasicMapMarker bmMarker : qt_BasicMapMarkers) {
                if (bmMarker) BasicMap().RemoveMarker(BasicMap().SERVER_KEY, bmMarker);
            }
            qt_BasicMapMarkers.Clear();
        #endif
    }
}
