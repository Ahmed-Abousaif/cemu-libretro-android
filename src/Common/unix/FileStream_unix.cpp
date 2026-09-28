#include "Common/unix/FileStream_unix.h"
#include <cstdarg>
#include <fcntl.h>
#include <unistd.h>

fs::path findPathCI(const fs::path& path)
{
	if (fs::exists(path)) return path;

	fs::path fName = path.filename();
	fs::path parentPath = path.parent_path();
	if (parentPath.empty())
		parentPath = ".";
	else if (!fs::exists(parentPath))
		parentPath = findPathCI(parentPath);

	std::error_code listErr;
	for (auto&& dirEntry : fs::directory_iterator(parentPath, listErr))
		if (boost::iequals(dirEntry.path().filename().string(), fName.string()))
			return dirEntry;

	return parentPath / fName;
}

FileStreamUnix* FileStreamUnix::openFile(std::string_view path)
{
	return openFile2(path, false);
}

FileStreamUnix* FileStreamUnix::openFile(const wchar_t* path, bool allowWrite)
{
	return openFile2(path, allowWrite);
}

FileStreamUnix* FileStreamUnix::openFile2(const fs::path& path, bool allowWrite)
{
	FileStreamUnix* fs = new FileStreamUnix(path, true, allowWrite);
	if (fs->m_isValid)
		return fs;
	delete fs;
	return nullptr;
}

FileStreamUnix* FileStreamUnix::createFile(const wchar_t* path)
{
	return createFile2(path);
}

FileStreamUnix* FileStreamUnix::createFile(std::string_view path)
{
	return createFile2(path);
}

FileStreamUnix* FileStreamUnix::createFile2(const fs::path& path)
{
	FileStreamUnix* fs = new FileStreamUnix(path, false, false);
	if (fs->m_isValid)
		return fs;
	delete fs;
	return nullptr;
}

std::optional<std::vector<uint8>> FileStreamUnix::LoadIntoMemory(const fs::path& path)
{
	FileStreamUnix* fs = openFile2(path);
	if (!fs)
		return std::nullopt;
	uint64 fileSize = fs->GetSize();
	if (fileSize > 0xFFFFFFFFull)
	{
		delete fs;
		return std::nullopt;
	}
	std::optional<std::vector<uint8>> v(fileSize);
	if (fs->readData(v->data(), (uint32)fileSize) != (uint32)fileSize)
	{
		delete fs;
		return std::nullopt;
	}
	delete fs;
	return v;
}

void FileStreamUnix::SetPosition(uint64 pos)
{
	cemu_assert(m_isValid);
	if (m_prevOperationWasWrite)
		m_fileStream.seekp((std::streampos)pos);
	else
		m_fileStream.seekg((std::streampos)pos);
}

uint64 FileStreamUnix::GetSize()
{
	cemu_assert(m_isValid);
	auto currentPos = m_fileStream.tellg();
	m_fileStream.seekg(0, std::ios::end);
	auto fileSize = m_fileStream.tellg();
	m_fileStream.seekg(currentPos, std::ios::beg);
	uint64 fs = (uint64)fileSize;
	return fs;
}

bool FileStreamUnix::SetEndOfFile()
{
	if (!m_isValid)
		return false;
	const std::streampos pos = m_prevOperationWasWrite ? m_fileStream.tellp() : m_fileStream.tellg();
	if (pos < 0)
		return false;
	m_fileStream.flush();
	if (!m_fileStream)
		return false;
	std::error_code ec;
	fs::resize_file(m_path, static_cast<std::uintmax_t>(pos), ec);
	if (ec)
	{
		cemuLog_log(LogType::Force, "SetEndOfFile failed: {}", ec.message());
		return false;
	}
	m_fileStream.clear();
	m_fileStream.seekg(pos);
	m_fileStream.seekp(pos);
	m_dirty = true;
	if (!SyncToDisk())
		return false;
	m_dirty = false;
	return true;
}

void FileStreamUnix::extract(std::vector<uint8>& data)
{
	uint64 fileSize = GetSize();
	SetPosition(0);
	data.resize(fileSize);
	readData(data.data(), fileSize);
}

bool FileStreamUnix::SyncToDisk()
{
	m_fileStream.flush();
	if (!m_fileStream || m_path.empty())
		return false;
	const int fd = ::open(m_path.c_str(), O_RDONLY);
	if (fd < 0)
		return false;
	const int synced = ::fsync(fd);
	::close(fd);
	return synced == 0;
}

void FileStreamUnix::Flush()
{
	if (!m_isValid)
		return;
	if (!m_dirty)
	{
		m_fileStream.flush();
		return;
	}
	if (SyncToDisk())
		m_dirty = false;
	else
		cemuLog_log(LogType::Force, "Failed to commit file to storage");
}

uint32 FileStreamUnix::readData(void* data, uint32 length)
{
	SyncReadWriteSeek(false);
	m_fileStream.read((char*)data, length);
	size_t bytesRead = m_fileStream.gcount();
	return (uint32)bytesRead;
}

bool FileStreamUnix::readU64(uint64& v)
{
	return readData(&v, sizeof(uint64)) == sizeof(uint64);
}

bool FileStreamUnix::readU32(uint32& v)
{
	return readData(&v, sizeof(uint32)) == sizeof(uint32);
}

bool FileStreamUnix::readU8(uint8& v)
{
	return readData(&v, sizeof(uint8)) == sizeof(uint8);
}

bool FileStreamUnix::readLine(std::string& line)
{
	line.clear();
	uint8 c;
	bool isEOF = true;
	while (readU8(c))
	{
		isEOF = false;
		if (c == '\r')
			continue;
		if (c == '\n')
			break;
		line.push_back((char)c);
	}
	return !isEOF;
}

sint32 FileStreamUnix::writeData(const void* data, sint32 length)
{
	if (length <= 0)
		return 0;
	SyncReadWriteSeek(true);
	m_fileStream.write(static_cast<const char*>(data), length);
	if (!m_fileStream)
		return 0;
	m_dirty = true;
	return length;
}

void FileStreamUnix::writeU64(uint64 v)
{
	writeData(&v, sizeof(uint64));
}

void FileStreamUnix::writeU32(uint32 v)
{
	writeData(&v, sizeof(uint32));
}

void FileStreamUnix::writeU8(uint8 v)
{
	writeData(&v, sizeof(uint8));
}

void FileStreamUnix::writeStringFmt(const char* format, ...)
{
	char buffer[2048];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	writeData(buffer, (sint32)strlen(buffer));
}

void FileStreamUnix::writeString(const char* str)
{
	writeData(str, (sint32)strlen(str));
}

void FileStreamUnix::writeLine(const char* str)
{
	writeData(str, (sint32)strlen(str));
	writeData("\r\n", 2);
}

FileStreamUnix::~FileStreamUnix()
{
	if (m_isValid)
	{
		if (m_dirty)
			Flush();
		m_fileStream.close();
		m_isValid = false;
	}
}

FileStreamUnix::FileStreamUnix(const fs::path& path, bool isOpen, bool isWriteable)
{
	fs::path CIPath = findPathCI(path);
	m_path = CIPath;
	if (isOpen)
	{
		m_fileStream.open(CIPath, isWriteable ? (std::ios_base::in | std::ios_base::out | std::ios_base::binary) : (std::ios_base::in | std::ios_base::binary));
		m_isValid = m_fileStream.is_open();
	}
	else
	{
		m_fileStream.open(CIPath, std::ios_base::in | std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
		m_isValid = m_fileStream.is_open();
	}
	if(m_isValid && fs::is_directory(path))
	{
		m_isValid = false;
		m_fileStream.close();
	}
	// Creating a file truncates it in the kernel. That change still has to be committed.
	if (m_isValid && !isOpen)
		m_dirty = true;
}

void FileStreamUnix::SyncReadWriteSeek(bool nextOpIsWrite)
{
	// nextOpIsWrite == false -> read. Otherwise write
	if (nextOpIsWrite == m_prevOperationWasWrite)
		return;
	if (nextOpIsWrite)
		m_fileStream.seekp(m_fileStream.tellg(), std::ios::beg);
	else
		m_fileStream.seekg(m_fileStream.tellp(), std::ios::beg);

	m_prevOperationWasWrite = nextOpIsWrite;
}
