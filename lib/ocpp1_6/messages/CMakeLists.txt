
target_sources(ocpp
    PRIVATE
        Authorize.cpp
        BootNotification.cpp
        CancelReservation.cpp
        ChangeAvailability.cpp
        ChangeConfiguration.cpp
        ClearCache.cpp
        ClearChargingProfile.cpp
        DataTransfer.cpp
        DiagnosticsStatusNotification.cpp
        FirmwareStatusNotification.cpp
        GetCompositeSchedule.cpp
        GetConfiguration.cpp
        GetDiagnostics.cpp
        GetLocalListVersion.cpp
        Heartbeat.cpp
        MeterValues.cpp
        RemoteStartTransaction.cpp
        RemoteStopTransaction.cpp
        ReserveNow.cpp
        Reset.cpp
        SendLocalList.cpp
        SetChargingProfile.cpp
        StartTransaction.cpp
        StatusNotification.cpp
        StopTransaction.cpp
        TriggerMessage.cpp
        UnlockConnector.cpp
        UpdateFirmware.cpp
        CertificateSigned.cpp
        DeleteCertificate.cpp
        ExtendedTriggerMessage.cpp
        GetInstalledCertificateIds.cpp
        GetLog.cpp
        InstallCertificate.cpp
        LogStatusNotification.cpp
        SecurityEventNotification.cpp
        SignCertificate.cpp
        SignedFirmwareStatusNotification.cpp
        SignedUpdateFirmware.cpp
    )