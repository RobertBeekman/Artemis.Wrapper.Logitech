#pragma once
class ArtemisPipeClient
{
private:
	bool isConnected = false;
	HANDLE _pipe = NULL;
public:
	bool IsConnected();
	void Connect();
	void Disconnect();
	void Write(LPCVOID data, DWORD length);
};

