#include "../lib/protocol.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace MessagingProtocol;

namespace {

struct ByteReader {
  const uint8_t *data;
  size_t size;
  size_t offset = 0;

  uint8_t u8() { return offset >= size ? 0 : data[offset++]; }
  uint32_t u32() { uint32_t v = 0; for (int i = 0; i < 4; i++) v = (v << 8) | u8(); return v; }
  size_t remaining() const { return offset < size ? size - offset : 0; }

  std::string bounded_string(size_t max_len) {
    size_t len = std::min(static_cast<size_t>(u8()), max_len);
    len = std::min(len, remaining());
    std::string s(reinterpret_cast<const char *>(data + offset), len);
    offset += len;
    return s;
  }
};

Message *build_chat_message(ByteReader &r) {
  Message *msg = new Message(CHAT_MESSAGE);
  msg->chat->username.set_data(r.bounded_string(64));
  msg->chat->message.set_data(r.bounded_string(255));
  msg->chat->timestamp = r.u32();
  msg->chat->priority = r.u8();
  return msg;
}

Message *build_user_info(ByteReader &r) {
  Message *msg = new Message(USER_INFO);
  msg->user_info->username.set_data(r.bounded_string(64));
  msg->user_info->email.set_data(r.bounded_string(64));
  msg->user_info->user_id = r.u32();
  msg->user_info->status = r.u8();

  uint32_t requested_tags = r.u8() % 5;
  std::vector<std::string> tags;
  for (uint32_t i = 0; i < requested_tags && r.remaining() > 0; i++) {
    tags.push_back(r.bounded_string(32));
  }
  msg->user_info->tag_count = static_cast<uint32_t>(tags.size());
  if (!tags.empty()) {
    msg->user_info->tags = new ProtocolString[tags.size()];
    for (size_t i = 0; i < tags.size(); i++) msg->user_info->tags[i].set_data(tags[i]);
  }
  return msg;
}

Message *build_file_chunk(ByteReader &r) {
  Message *msg = new Message(FILE_CHUNK);
  msg->file_chunk->filename.set_data(r.bounded_string(64));
  msg->file_chunk->chunk_id = r.u32();
  msg->file_chunk->total_chunks = r.u32();

  uint16_t chunk_size = static_cast<uint16_t>(std::min(r.remaining(), static_cast<size_t>(4096)));
  msg->file_chunk->chunk_size = chunk_size;
  if (chunk_size > 0) {
    msg->file_chunk->data = new uint8_t[chunk_size];
    memcpy(msg->file_chunk->data, r.data + r.offset, chunk_size);
    r.offset += chunk_size;
  }
  return msg;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 2) return 0;

  ByteReader r{data, size};
  uint8_t selector = r.u8();
  Message *original = nullptr;

  switch (selector % 3) {
  case 0: original = build_chat_message(r); break;
  case 1: original = build_user_info(r); break;
  case 2: original = build_file_chunk(r); break;
  }
  if (!original) return 0;

  std::vector<uint8_t> serialized = Serializer::serialize(*original);
  Message *roundtrip = Serializer::deserialize(serialized.data(), serialized.size());
  assert(roundtrip != nullptr);
  assert(roundtrip->header.type == original->header.type);

  switch (original->header.type) {
  case CHAT_MESSAGE:
    assert(roundtrip->chat->username.to_string() == original->chat->username.to_string());
    assert(roundtrip->chat->message.to_string() == original->chat->message.to_string());
    assert(roundtrip->chat->timestamp == original->chat->timestamp);
    assert(roundtrip->chat->priority == original->chat->priority);
    break;
  case USER_INFO:
    assert(roundtrip->user_info->username.to_string() == original->user_info->username.to_string());
    assert(roundtrip->user_info->email.to_string() == original->user_info->email.to_string());
    assert(roundtrip->user_info->tag_count == original->user_info->tag_count);
    for (uint32_t i = 0; i < original->user_info->tag_count; i++) {
      assert(roundtrip->user_info->tags[i].to_string() == original->user_info->tags[i].to_string());
    }
    break;
  case FILE_CHUNK:
    assert(roundtrip->file_chunk->filename.to_string() == original->file_chunk->filename.to_string());
    assert(roundtrip->file_chunk->chunk_size == original->file_chunk->chunk_size);
    if (original->file_chunk->chunk_size > 0) {
      assert(memcmp(roundtrip->file_chunk->data, original->file_chunk->data, original->file_chunk->chunk_size) == 0);
    }
    break;
  default:
    break;
  }

  delete roundtrip;
  delete original;
  return 0;
}