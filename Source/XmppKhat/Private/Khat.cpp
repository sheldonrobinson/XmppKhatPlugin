// (c) 2015 Descendent Studios, Inc.

#include "Khat.h"

#include "Modules/ModuleManager.h"
#include "Xmpp.h"
#include "XmppConnection.h"

DEFINE_LOG_CATEGORY(LogChat);

UKhatMember::UKhatMember(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer),
	Status(EUXmppPresenceStatus::Offline),
	bIsAvailable(false),
	Affiliation(EUKhatMemberAffiliation::Member),
	Role(EUKhatMemberRole::Participant)
{
}

void UKhatMember::ConvertFrom(const FXmppChatMember& ChatMember)
{
	Nickname = ChatMember.Nickname;
	MemberJid = ChatMember.UserJid.GetFullPath();
	Status = UKhatUtil::GetEUXmppPresenceStatus(ChatMember.UserPresence.Status);
	bIsAvailable = ChatMember.UserPresence.bIsAvailable;
	SentTime = ChatMember.UserPresence.SentTime;
	StatusStr = ChatMember.UserPresence.StatusStr;
	Affiliation = UKhatUtil::GetEUKhatMemberAffiliation(ChatMember.Affiliation);
	Role = UKhatUtil::GetEUKhatMemberRole(ChatMember.Role);
}

/***************** Base **************************/

UKhat::UKhat(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), bInited(false), bDone(false)
{
}

UKhat::~UKhat()
{
	DeInit();
}

void UKhat::Init()
{
	if (XmppConnection.IsValid() && !bInited)
	{
		bInited = true;

		IXmppConnection::FOnXmppLoginComplete& OnXMPPLoginCompleteDelegate = XmppConnection->OnLoginComplete();
		OnLoginCompleteHandle = OnXMPPLoginCompleteDelegate.AddUObject(this, &UKhat::OnLoginCompleteFunc);

		IXmppConnection::FOnXmppLogoutComplete& OnXMPPLogoutCompleteDelegate = XmppConnection->OnLogoutComplete();
		OnLogoutCompleteHandle = OnXMPPLogoutCompleteDelegate.AddUObject(this, &UKhat::OnLogoutCompleteFunc);

		IXmppConnection::FOnXmppLogingChanged& OnXMPPLogingChangedDelegate = XmppConnection->OnLoginChanged();
		OnLogingChangedHandle = OnXMPPLogingChangedDelegate.AddUObject(this, &UKhat::OnLoginChangedFunc);

		if (XmppConnection->Messages().IsValid())
		{
			IXmppMessages::FOnXmppMessageReceived& OnXMPPReceiveMessageDelegate = XmppConnection->Messages()->OnReceiveMessage();
			OnChatReceiveMessageHandle = OnXMPPReceiveMessageDelegate.AddUObject(this, &UKhat::OnChatReceiveMessageFunc);
		}

		if (XmppConnection->PrivateChat().IsValid())
		{
			IXmppChat::FOnXmppChatReceived& OnXmppKhatReceivedDelegate = XmppConnection->PrivateChat()->OnReceiveChat();
			OnPrivateChatReceiveMessageHandle = OnXmppKhatReceivedDelegate.AddUObject(this, &UKhat::OnPrivateChatReceiveMessageFunc);
		}

		if (XmppConnection->MultiUserChat().IsValid())
		{
			IXmppMultiUserChat::FOnXmppRoomChatReceived& OnXMPPMUCReceiveMessageDelegate = XmppConnection->MultiUserChat()->OnRoomChatReceived();
			OnMUCReceiveMessageHandle = OnXMPPMUCReceiveMessageDelegate.AddUObject(this, &UKhat::OnMUCReceiveMessageFunc);

			IXmppMultiUserChat::FOnXmppRoomJoinPublicComplete& OnXMPPMUCRoomJoinPublicDelegate = XmppConnection->MultiUserChat()->OnJoinPublicRoom();
			OnMUCRoomJoinPublicCompleteHandle = OnXMPPMUCRoomJoinPublicDelegate.AddUObject(this, &UKhat::OnMUCRoomJoinPublicCompleteFunc);

			IXmppMultiUserChat::FOnXmppRoomJoinPrivateComplete& OnXMPPMUCRoomJoinPrivateDelegate = XmppConnection->MultiUserChat()->OnJoinPrivateRoom();
			OnMUCRoomJoinPrivateCompleteHandle = OnXMPPMUCRoomJoinPrivateDelegate.AddUObject(this, &UKhat::OnMUCRoomJoinPrivateCompleteFunc);

			IXmppMultiUserChat::FOnXmppRoomMemberJoin& OnXMPPRoomMemberJoinDelegate = XmppConnection->MultiUserChat()->OnRoomMemberJoin();
			OnMUCRoomMemberJoinHandle = OnXMPPRoomMemberJoinDelegate.AddUObject(this, &UKhat::OnMUCRoomMemberJoinFunc);

			IXmppMultiUserChat::FOnXmppRoomMemberExit& OnXMPPRoomMemberExitDelegate = XmppConnection->MultiUserChat()->OnRoomMemberExit();
			OnMUCRoomMemberExitHandle = OnXMPPRoomMemberExitDelegate.AddUObject(this, &UKhat::OnMUCRoomMemberExitFunc);

			IXmppMultiUserChat::FOnXmppRoomMemberChanged& OnXMPPRoomMemberChangedDelegate = XmppConnection->MultiUserChat()->OnRoomMemberChanged();
			OnMUCRoomMemberChangedHandle = OnXMPPRoomMemberChangedDelegate.AddUObject(this, &UKhat::OnMUCRoomMemberChangedFunc);
		}
	}
}

void UKhat::DeInit()
{
	if (XmppConnection.IsValid())
	{
		bInited = false;

		if (OnLoginCompleteHandle.IsValid()) { XmppConnection->OnLoginComplete().Remove(OnLoginCompleteHandle); }
		if (OnLogoutCompleteHandle.IsValid()) { XmppConnection->OnLogoutComplete().Remove(OnLogoutCompleteHandle); }
		if (OnLogingChangedHandle.IsValid()) { XmppConnection->OnLoginChanged().Remove(OnLogingChangedHandle); }
		if (OnChatReceiveMessageHandle.IsValid()) { XmppConnection->Messages()->OnReceiveMessage().Remove(OnChatReceiveMessageHandle); }
		if (OnPrivateChatReceiveMessageHandle.IsValid()) { XmppConnection->PrivateChat()->OnReceiveChat().Remove(OnPrivateChatReceiveMessageHandle); }
		if (OnMUCReceiveMessageHandle.IsValid()) { XmppConnection->MultiUserChat()->OnRoomChatReceived().Remove(OnMUCReceiveMessageHandle); }
		if (OnMUCRoomJoinPublicCompleteHandle.IsValid()) { XmppConnection->MultiUserChat()->OnJoinPublicRoom().Remove(OnMUCRoomJoinPublicCompleteHandle); }
		if (OnMUCRoomJoinPrivateCompleteHandle.IsValid()) { XmppConnection->MultiUserChat()->OnJoinPrivateRoom().Remove(OnMUCRoomJoinPrivateCompleteHandle); }
		if (OnMUCRoomMemberJoinHandle.IsValid()) { XmppConnection->MultiUserChat()->OnRoomMemberJoin().Remove(OnMUCRoomMemberJoinHandle); }
		if (OnMUCRoomMemberExitHandle.IsValid()) { XmppConnection->MultiUserChat()->OnRoomMemberExit().Remove(OnMUCRoomMemberExitHandle); }
		if (OnMUCRoomMemberChangedHandle.IsValid()) { XmppConnection->MultiUserChat()->OnRoomMemberChanged().Remove(OnMUCRoomMemberChangedHandle); }

		FXmppModule::Get().RemoveConnection(XmppConnection.ToSharedRef());
	}	
}

void UKhat::Finish()
{
	if (XmppConnection.IsValid())
	{
		if (XmppConnection->GetLoginStatus() == EXmppLoginStatus::LoggedIn)
		{
			Logout();
		}
		else
		{
			bDone = true;
		}
	}
}

/***************** Login/Logout **************************/

void UKhat::Login(const FString& UserId, const FString& Auth, const FString& ServerAddr, const FString& Domain, const FString& ClientResource)
{
	FXmppServer XmppServer;
	XmppServer.ServerAddr = ServerAddr;
	XmppServer.Domain = Domain;
	XmppServer.ClientResource = ClientResource;

	Login(UserId, Auth, XmppServer);
}

void UKhat::Login(const FString& UserId, const FString& Auth, const FXmppServer& XmppServer)
{
	FXmppModule& Module = FModuleManager::GetModuleChecked<FXmppModule>("XMPP");

	UE_LOG(LogChat, Log, TEXT("UKhat::Login enabled=%s UserId=%s"), (Module.IsXmppEnabled() ? TEXT("true") : TEXT("false")), *UserId );

	XmppConnection = Module.CreateConnection(UserId);

	if (XmppConnection.IsValid())
	{
		//UE_LOG(LogChat, Log, TEXT("UKhat::Login XmppConnection is %s"), typeid(*XmppConnection).name() );
		UE_LOG(LogChat, Log, TEXT("UKhat::Login XmppConnection valid") );

		Init();

		XmppConnection->SetServer(XmppServer);

		XmppConnection->Login(UserId, Auth);
	}
	else
	{
		UE_LOG(LogChat, Error, TEXT("UKhat::Login XmppConnection not valid, failed.  UserId=%s"), *UserId );
	}
}

void UKhat::OnLoginCompleteFunc(const FXmppUserJid& UserJid, bool bWasSuccess, const FString& Error)
{
	UE_LOG(LogChat, Log, TEXT("UKhat::OnLoginComplete UserJid=%s Success=%s Error=%s"),	*UserJid.GetFullPath(), bWasSuccess ? TEXT("true") : TEXT("false"), *Error);

	OnChatLoginComplete.Broadcast(UserJid.GetFullPath(), bWasSuccess, Error);
}

void UKhat::OnLogoutCompleteFunc(const FXmppUserJid& UserJid, bool bWasSuccess, const FString& Error)
{
	UE_LOG(LogChat, Log, TEXT("UKhat::OnLogoutComplete UserJid=%s Success=%s Error=%s"), *UserJid.GetFullPath(), bWasSuccess ? TEXT("true") : TEXT("false"), *Error);	

	OnChatLogoutComplete.Broadcast(UserJid.GetFullPath(), bWasSuccess, Error);
}

void UKhat::OnLoginChangedFunc(const FXmppUserJid& UserJid, EXmppLoginStatus::Type LoginStatus)
{
	UE_LOG(LogChat, Log, TEXT("UKhat::OnLogingChanged UserJid=%s LoginStatus=%d"), *UserJid.GetFullPath(), static_cast<int32>(LoginStatus));

	OnChatLogingChanged.Broadcast(UserJid.GetFullPath(), UKhatUtil::GetEUXmppLoginStatus(LoginStatus));
}

void UKhat::Logout()
{
	if (XmppConnection.IsValid() && (XmppConnection->GetLoginStatus() == EXmppLoginStatus::LoggedIn))
	{
		XmppConnection->Logout();
	}
}

/***************** Chat **************************/

void UKhat::OnChatReceiveMessageFunc(const TSharedRef<IXmppConnection>& Connection, const FXmppUserJid& FromJid, const TSharedRef<FXmppMessage>& Message)
{
	UE_LOG(LogChat, Log, TEXT("UKhat::OnChatReceiveMessage UserJid=%s Type=%s Message=%s"), *FromJid.GetFullPath(), *Message->Type, *Message->Payload);

	OnChatReceiveMessage.Broadcast(FromJid.GetFullPath(), Message->Type, Message->Payload);
}

void UKhat::OnPrivateChatReceiveMessageFunc(const TSharedRef<IXmppConnection>& Connection, const FXmppUserJid& FromJid, const TSharedRef<FXmppChatMessage>& Message)
{
	UE_LOG(LogChat, Log, TEXT("UKhat::OnPrivateChatReceiveMessage UserJid=%s Message=%s"), *FromJid.GetFullPath(), *Message->Body);

	OnPrivateChatReceiveMessage.Broadcast(FromJid.GetFullPath(), Message->Body);
}

void UKhat::Message(const FString& UserName, const FString& Recipient, const FString& Type, const FString& MessagePayload)
{
	if (XmppConnection->Messages().IsValid())
	{
		FXmppUserJid RecipientId(Recipient, XmppConnection->GetServer().Domain);
		XmppConnection->Messages()->SendMessage(RecipientId, Type, MessagePayload);
	}
}

void UKhat::PrivateChat(const FString& UserName, const FString& Recipient, const FString& Body)
{
	if (XmppConnection->PrivateChat().IsValid())
	{
		FXmppUserJid RecipientId(Recipient, XmppConnection->GetServer().Domain);
		XmppConnection->PrivateChat()->SendChat(RecipientId, Body);
	}
}


/***************** Presence **************************/

void UKhat::Presence(bool bIsAvailable, EUXmppPresenceStatus::Type Status, const FString& StatusStr)
{
	if (XmppConnection->Presence().IsValid())
	{		
		FXmppUserPresence XmppPresence = XmppConnection->Presence()->GetPresence();
		XmppPresence.bIsAvailable = bIsAvailable;
		XmppPresence.Status = UKhatUtil::GetEXmppPresenceStatus(Status);
		XmppConnection->Presence()->UpdatePresence(XmppPresence);
	}
}

void UKhat::PresenceQuery(const FString& User)
{
	if (XmppConnection->Presence().IsValid())
	{
		XmppConnection->Presence()->QueryPresence(User);
	}
}

void UKhat::PresenceGetRosterMembers(TArray<FString>& Members)
{
	if (XmppConnection->Presence().IsValid())
	{
		TArray<FXmppUserJid> MemberJids;
		XmppConnection->Presence()->GetRosterMembers(MemberJids);

		for (auto& Jid : MemberJids)
		{			
			Members.Push(Jid.Id);
		}
	}
}

// TODO:
// FXmppUserPresenceJingle and FXmppUserPresence
// TArray<TSharedPtr<FXmppUserPresence>> FXmppPresenceJingle::GetRosterPresence(const FString& UserId)
// virtual FOnXmppPresenceReceived& OnReceivePresence() override { return OnXmppPresenceReceivedDelegate; }
// Needed?
// 	virtual bool UpdatePresence(const FXmppUserPresence& Presence) override;
//	virtual const FXmppUserPresence& GetPresence() const override;

/***************** MUC **************************/

void UKhat::OnMUCReceiveMessageFunc(const TSharedRef<IXmppConnection>& Connection, const FXmppRoomId& RoomId, const FXmppUserJid& UserJid, const TSharedRef<FXmppChatMessage>& ChatMsg)
{
	if (Connection->MultiUserChat().IsValid())
	{
		OnMUCReceiveMessage.Broadcast(static_cast<FString>(RoomId), UserJid.Resource, *ChatMsg->Body);
	}
}

void UKhat::OnMUCRoomJoinPublicCompleteFunc(const TSharedRef<IXmppConnection>& Connection, bool bSuccess, const FXmppRoomId& RoomId, const FString& Error)
{
	OnMUCRoomJoinPublicComplete.Broadcast(bSuccess, static_cast<FString>(RoomId), Error);
}

void UKhat::OnMUCRoomJoinPrivateCompleteFunc(const TSharedRef<IXmppConnection>& Connection, bool bSuccess, const FXmppRoomId& RoomId, const FString& Error)
{
	OnMUCRoomJoinPrivateComplete.Broadcast(bSuccess, static_cast<FString>(RoomId), Error);
}

void UKhat::OnMUCRoomMemberJoinFunc(const TSharedRef<IXmppConnection>& Connection, const FXmppRoomId& RoomId, const FXmppUserJid& UserJid)
{	
	UE_LOG(LogChat, Log, TEXT("UKhat::OnMUCRoomMemberJoin RoomId=%s UserJid=%s"), *static_cast<FString>(RoomId), *UserJid.GetFullPath());
	OnMUCRoomMemberJoin.Broadcast(static_cast<FString>(RoomId), UserJid.Resource);
}

void UKhat::OnMUCRoomMemberExitFunc(const TSharedRef<IXmppConnection>& Connection, const FXmppRoomId& RoomId, const FXmppUserJid& UserJid)
{
	UE_LOG(LogChat, Log, TEXT("UKhat::OnMUCRoomMemberExit RoomId=%s UserJid=%s"), *static_cast<FString>(RoomId), *UserJid.GetFullPath());
	OnMUCRoomMemberExit.Broadcast(static_cast<FString>(RoomId), UserJid.Resource);
}

void UKhat::OnMUCRoomMemberChangedFunc(const TSharedRef<IXmppConnection>& Connection, const FXmppRoomId& RoomId, const FXmppUserJid& UserJid)
{
	UE_LOG(LogChat, Log, TEXT("UKhat::OnMUCRoomMemberChanged RoomId=%s UserJid=%s"), *static_cast<FString>(RoomId), *UserJid.GetFullPath());
	OnMUCRoomMemberChanged.Broadcast(static_cast<FString>(RoomId), UserJid.Resource);
}

void UKhat::MucCreate(const FString& UserName, const FString& RoomId, bool bIsPrivate, const FString& Password)
{
	if (XmppConnection.IsValid() && XmppConnection->MultiUserChat().IsValid())
	{
		FXmppRoomConfig RoomConfig;
		RoomConfig.RoomName = RoomId;		
		RoomConfig.bIsPersistent = false;
		RoomConfig.bIsPrivate = bIsPrivate;		
		RoomConfig.Password = Password;
		XmppConnection->MultiUserChat()->CreateRoom(RoomId, UserName, RoomConfig);
	}
}

void UKhat::MucJoin(const FString& RoomId, const FString& Nickname, const FString& Password)
{
	if (XmppConnection.IsValid() && XmppConnection->MultiUserChat().IsValid())
	{
		if (Password.IsEmpty())
		{
			XmppConnection->MultiUserChat()->JoinPublicRoom(RoomId, Nickname);
		}
		else
		{
			XmppConnection->MultiUserChat()->JoinPrivateRoom(RoomId, Nickname, Password);
		}
	}
}

void UKhat::MucExit(const FString& RoomId)
{
	if (XmppConnection.IsValid() && XmppConnection->MultiUserChat().IsValid())
	{
		XmppConnection->MultiUserChat()->ExitRoom(RoomId);
	}
}

void UKhat::MucChat(const FString& RoomId, const FString& Body, const FString& ExtraInfo)
{				
	if (XmppConnection.IsValid() && XmppConnection->MultiUserChat().IsValid())
	{
		XmppConnection->MultiUserChat()->SendChat(RoomId, Body, ExtraInfo);
	}
}

void UKhat::MucConfig(const FString& UserName, const FString& RoomId, bool bIsPrivate, const FString& Password)
{
	if (XmppConnection.IsValid() && XmppConnection->MultiUserChat().IsValid())
	{
		FXmppRoomConfig RoomConfig;
		RoomConfig.bIsPrivate = bIsPrivate;
		RoomConfig.Password = Password;
		XmppConnection->MultiUserChat()->ConfigureRoom(RoomId, RoomConfig);
	}
}

void UKhat::MucRefresh(const FString& RoomId)
{
	if (XmppConnection.IsValid() && XmppConnection->MultiUserChat().IsValid())
	{
		XmppConnection->MultiUserChat()->RefreshRoomInfo(RoomId);
	}
}

void UKhat::MucGetMembers(const FString& RoomId, TArray<UKhatMember*>& Members)
{
	if (XmppConnection.IsValid() && XmppConnection->MultiUserChat().IsValid())
	{
		TArray< FXmppChatMemberRef > OutMembers;
		XmppConnection->MultiUserChat()->GetMembers(RoomId, OutMembers);

		Members.Empty();
		Members.Reserve(OutMembers.Num());
		for (auto& Member : OutMembers)
		{
			UKhatMember* UMember = NewObject<UKhatMember>();
			UMember->ConvertFrom(Member.Get());
			Members.Add(UMember);
		}
	}
}

/***************** PubSub **************************/

void UKhat::PubSubCreate(const FString& NodeId)
{
	if (XmppConnection.IsValid() && XmppConnection->PubSub().IsValid())
	{
		FXmppPubSubConfig PubSubConfig;
		XmppConnection->PubSub()->CreateNode(NodeId, PubSubConfig);
	}
}

void UKhat::PubSubDestroy(const FString& NodeId)
{
	if (XmppConnection.IsValid() && XmppConnection->PubSub().IsValid())
	{
		XmppConnection->PubSub()->DestroyNode(NodeId);
	}
}

void UKhat::PubSubSubscribe(const FString& NodeId)
{
	if (XmppConnection.IsValid() && XmppConnection->PubSub().IsValid())
	{
		XmppConnection->PubSub()->Subscribe(NodeId);
	}
}

void UKhat::PubSubUnsubscribe(const FString& NodeId)
{
	if (XmppConnection.IsValid() && XmppConnection->PubSub().IsValid())
	{
		XmppConnection->PubSub()->Unsubscribe(NodeId);
	}
}

void UKhat::PubSubPublish(const FString& NodeId, const FString& Payload)
{
	if (XmppConnection.IsValid() && XmppConnection->PubSub().IsValid())
	{
		FXmppPubSubMessage Message;
		Message.Payload = Payload;
		XmppConnection->PubSub()->PublishMessage(NodeId, Message);
	}
}



