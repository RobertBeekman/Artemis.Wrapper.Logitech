#include "pch.h"
#include "ArtemisPipeClient.h"
#include "Logger.h"
#include "Constants.h"

bool ArtemisPipeClient::IsConnected()
{
	return isConnected;
}

void ArtemisPipeClient::Connect()
{
	LOG("Connecting to pipe...");

	_pipe = CreateFile(
		PIPE_NAME,
		GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	isConnected = _pipe != NULL && _pipe != INVALID_HANDLE_VALUE;

	LOG(isConnected ? "Connected to pipe successfully" : "Could not connect to pipe");
}

void ArtemisPipeClient::Disconnect()
{
	LOG("Closing pipe...");
	CloseHandle(_pipe);
	isConnected = false;
	LOG("Closed pipe");
}

void ArtemisPipeClient::Write(LPCVOID data, DWORD length)
{
	if (!isConnected) {
		LOG("Pipe disconnected when writing, trying to reconnect...");
		Connect();
		if (!isConnected) {
			return;
		}
		LOG("Pipe connection reestablished");
	}

	DWORD writtenLength;
	BOOL result = WriteFile(
		_pipe,
		data,
		length,
		&writtenLength,
		NULL);

	if ((!result) || (writtenLength < length)) {
		LOG(fmt::format("Error writing to pipe: \'{error}\'. Wrote {bytes} bytes out of {total}", result, writtenLength, length));
		Disconnect();
	}
}
