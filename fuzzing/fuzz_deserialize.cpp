#include "../lib/protocol.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace MessagingProtocol;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < sizeof(MessageHeader)) return 0;

  Message *msg = Serializer::deserialize(data, size);
  if (!msg) return 0;

  switch (msg->header.type) {
  case CHAT_MESSAGE:
    if (msg->chat) {
      volatile uint64_t ts = msg->chat->timestamp;
      volatile uint8_t pr = msg->chat->priority;
      std::string user = msg->chat->username.to_string();
      std::string text = msg->chat->message.to_string();
      (void)ts; (void)pr;
    }
    break;
  case USER_INFO:
    if (msg->user_info) {
      std::string user = msg->user_info->username.to_string();
      std::string email = msg->user_info->email.to_string();
      for (uint32_t i = 0; i < msg->user_info->tag_count; i++) {
        std::string tag = msg->user_info->tags[i].to_string();
        (void)tag;
      }
    }
    break;
  case FILE_CHUNK:
    if (msg->file_chunk) {
      std::string fn = msg->file_chunk->filename.to_string();
      if (msg->file_chunk->data && msg->file_chunk->chunk_size > 0) {
        volatile uint8_t first = msg->file_chunk->data[0];
        volatile uint8_t last = msg->file_chunk->data[msg->file_chunk->chunk_size - 1];
        (void)first; (void)last;
      }
    }
    break;
  default:
    break;
  }

  std::vector<uint8_t> reserialized = Serializer::serialize(*msg);
  (void)reserialized;
  delete msg;
  return 0;
}