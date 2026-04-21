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
            BXDServerMarker marker = BXDStaticMarkerManager.Get().AddTempServerMarker(def.name, def.position, "BXD_SystemParty\\gui\\icons\\player.paa", ARGB(255, 0, 255, 0));
            qt_SPMarkers.Insert(marker);
        #endif

        #ifdef LBmaster_Groups	
            #ifdef LBmaster_Rework
                LBServerMarker marker = LBStaticMarkerManager.Get.AddTempServerMarker(def.name, def.position, "LBmaster_Groups\\gui\\icons\\player.paa", ARGB(255, 0, 255, 0), false, true, true, true);
                qt_LBMarkers.Insert(marker);
            #endif
            #ifndef LBmaster_Rework
                LBServerMarker marker = LBStaticMarkerManager.Get().AddTempServerMarker(def.name, def.position, "LBmaster_Groups\\gui\\icons\\player.paa", ARGB(255, 0, 255, 0), false, true, true, true);
                qt_LBMarkers.Insert(marker);
            #endif
        #endif

        #ifdef EXPANSIONMODNAVIGATION
            ExpansionMarkerData marker = m_QT_TraderMarkerEvent.CreateServerMarker(def.name, "Territory", def.position, ARGB(255, 0, 255, 0), true);
            qt_ExpMarkers.Insert(marker);
        #endif
	}
	
	void DeleteMarker() {
        #ifdef BXD_SystemParty
            foreach (BXDMarker marker : qt_SPMarkers) {
                if(marker)  BXDStaticMarkerManager.Get().RemoveServerMarker(marker);
                qt_SPMarkers.RemoveItem(marker);                
            }
        #endif

        #ifdef LBmaster_Groups
            #ifdef LBmaster_Rework
                foreach (BXDMarker marker : qt_LBMarkers) {
                    if(marker) LBStaticMarkerManager.Get.RemoveServerMarker(qt_LBMarkers);
                    qt_LBMarkers.RemoveItem(marker);
                }
            #endif
            #ifndef LBmaster_Rework
                foreach (BXDMarker marker : qt_LBMarkers) {
                    if(marker) LBStaticMarkerManager.Get().RemoveServerMarker(qt_LBMarkers);
                    qt_LBMarkers.RemoveItem(marker);
                }
            #endif
        #endif

        #ifdef EXPANSIONMODNAVIGATION
            foreach (BXDMarker marker : qt_ExpMarkers) {
                if (marker) {
                    if (m_QT_TraderMarkerEvent) m_QT_TraderMarkerEvent.RemoveServerMarker(marker.GetUID());
                    m_QT_TraderMarkerEvent = NULL;
                    qt_ExpMarkers.RemoveItem(marker);
                }
            }
        #endif
	}
}