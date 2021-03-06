/******************************************************************************
* University of Illinois / NCSA
* Open Source License
*
* Copyright(c) 2011 - 2015  Advanced Micro Devices, Inc.
* All rights reserved.
*
* Developed by:
* Advanced Micro Devices, Inc.
* www.amd.com
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files(the "Software"), to deal
* with the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and /
* or sell copies of the Software, and to permit persons to whom the Software
* is furnished to do so, subject to the following conditions:
*
*     Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimers.
*
*     Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimers in the documentation
* and / or other materials provided with the distribution.
*
*     Neither the names of Advanced Micro Devices, Inc, nor the
mes of its
* contributors may be used to endorse or promote products derived from this
* Software without specific prior written permission.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
* THE SOFTWARE.
******************************************************************************/

#include "util.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <string>

#include <cstddef>
#include "xutil.hpp"
#include <libelf.h>

namespace ext {
namespace common {

//===----------------------------------------------------------------------===//
// ElfMemoryImage - Templates.                                                //
//===----------------------------------------------------------------------===//

template<typename ElfN_Ehdr, typename ElfN_Shdr>
uint64_t ElfMemoryImage::Size(const void *emi) {
  assert(IsValid(emi) && "elf memory image is not valid");

  const ElfN_Ehdr *ehdr = (const ElfN_Ehdr*)emi;
  if (NULL == ehdr || EV_CURRENT != ehdr->e_version) {
    return false;
  }

  const ElfN_Shdr *shdr = (const ElfN_Shdr*)((char*)emi + ehdr->e_shoff);
  if (NULL == shdr) {
    return false;
  }

  uint64_t max_offset = ehdr->e_shoff;
  uint64_t total_size = max_offset + ehdr->e_shentsize * ehdr->e_shnum;

  for (uint16_t i = 0; i < ehdr->e_shnum; ++i) {
    uint64_t cur_offset = static_cast<uint64_t>(shdr[i].sh_offset);
    if (max_offset < cur_offset) {
      max_offset = cur_offset;
      total_size = max_offset;
      if (SHT_NOBITS != shdr[i].sh_type) {
        total_size += static_cast<uint64_t>(shdr[i].sh_size);
      }
    }
  }

  return total_size;
}

//===----------------------------------------------------------------------===//
// ElfMemoryImage.                                                            //
//===----------------------------------------------------------------------===//

bool ElfMemoryImage::Is32(const void *emi) {
  if (!IsValid(emi)) {
    return false;
  }

  const unsigned char *e_ident = (const unsigned char*)emi;
  assert(NULL != e_ident && "elf identifier is null");
  return ELFCLASS32 == e_ident[EI_CLASS];
}

bool ElfMemoryImage::Is64(const void *emi) {
  if (!IsValid(emi)) {
    return false;
  }

  const unsigned char *e_ident = (const unsigned char*)emi;
  assert(NULL != e_ident && "elf identifier is null");
  return ELFCLASS64 == e_ident[EI_CLASS];
}

bool ElfMemoryImage::IsBigEndian(const void *emi) {
  if (!IsValid(emi)) {
    return false;
  }

  const unsigned char *e_ident = (const unsigned char*)emi;
  assert(NULL != e_ident && "elf identifier is null");
  return ELFDATA2MSB == e_ident[EI_DATA];
}

bool ElfMemoryImage::IsLittleEndian(const void *emi) {
  if (!IsValid(emi)) {
    return false;
  }

  const unsigned char *e_ident = (const unsigned char*)emi;
  assert(NULL != e_ident && "elf identifier is null");
  return ELFDATA2LSB == e_ident[EI_DATA];
}

bool ElfMemoryImage::IsValid(const void *emi) {
  if (NULL == emi) {
    return false;
  }

  const unsigned char *e_ident = (const unsigned char*)emi;
  if (NULL == e_ident) {
    return false;
  }

  if (ELFMAG0 != e_ident[EI_MAG0]) {
    return false;
  }
  if (ELFMAG1 != e_ident[EI_MAG1]) {
    return false;
  }
  if (ELFMAG2 != e_ident[EI_MAG2]) {
    return false;
  }
  if (ELFMAG3 != e_ident[EI_MAG3]) {
    return false;
  }

  return true;
}

uint64_t ElfMemoryImage::Size(const void *emi) {
  if (Is32(emi)) {
    return Size<Elf32_Ehdr, Elf32_Shdr>(emi);
  } else if (Is64(emi)) {
    return Size<Elf64_Ehdr, Elf64_Shdr>(emi);
  }
  return 0;
}

//===----------------------------------------------------------------------===//
// StringFactory.                                                             //
//===----------------------------------------------------------------------===//

std::string StringFactory::Flatten(const char **cstrs,
                                   const uint32_t &cstrs_count,
                                   const char &spacer) {
  if (NULL == cstrs || 0 == cstrs_count) {
    return std::string();
  }

  std::string flattened;
  for (uint32_t i = 0; i < cstrs_count; ++i) {
    if (NULL == cstrs[i]) {
      return std::string();
    }
    flattened += cstrs[i];
    if (i != (cstrs_count - 1)) {
      flattened += spacer;
    }
  }
  return flattened;
}

std::string StringFactory::Format(const char *cstr, va_list arg) {
  if (NULL == cstr) {
    return std::string();
  }

  va_list arg_copy;
  x_va_copy(arg_copy, arg);

  int32_t nbyte = x_vscprintf(cstr, arg);
  if (0 >= nbyte) {
    return std::string();
  }

  char *tmp_buf = (char*)calloc(nbyte + 1, sizeof(char));
  if (NULL == tmp_buf) {
    return std::string();
  }

  nbyte = x_vsnprintf(tmp_buf, nbyte + 1, cstr, arg_copy);
  if (0 >= nbyte) {
    free(tmp_buf);
    return std::string();
  }

  const std::string formatted = tmp_buf;
  free(tmp_buf);
  return formatted;
}

std::list<std::string> StringFactory::Tokenize(const char *cstr,
                                               const char &delim) {
  if (NULL == cstr) {
    return std::list<std::string>();
  }

  const std::string str = cstr;
  size_t start = 0;
  size_t end = 0;

  std::list<std::string> tokens;
  while ((end = str.find(delim, start)) != std::string::npos) {
    if (start != end) {
      tokens.push_back(str.substr(start, end - start));
    }
    start = end + 1;
  }
  if (str.size() > start) {
    tokens.push_back(str.substr(start));
  }
  return tokens;
}

std::string StringFactory::ToLower(const std::string& str) {
  std::string lower(str.length(), ' ');
  std::transform(str.begin(), str.end(), lower.begin(), ::tolower);
  return lower;
}

std::string StringFactory::ToUpper(const std::string& str) {
  std::string upper(str.length(), ' ');
  std::transform(str.begin(), str.end(), upper.begin(), ::toupper);
  return upper;
}

//===----------------------------------------------------------------------===//
// HelpPrinter, HelpStreambuf.                                                //
//===----------------------------------------------------------------------===//

HelpStreambuf::HelpStreambuf(std::ostream& stream)
  : basicStream_(&stream),
    basicBuf_(stream.rdbuf()),
    wrapWidth_(0),
    indentSize_(0),
    atLineStart_(true),
    lineWidth_(0)
{
  basicStream_->rdbuf(this);
}

HelpStreambuf::int_type HelpStreambuf::overflow(HelpStreambuf::int_type ch) {
    if (atLineStart_ && ch != '\n') {
      std::string indent(indentSize_, ' ');
      basicBuf_->sputn(indent.data(), indent.size());
      lineWidth_ = indentSize_;
      atLineStart_ = false;
    } else if (ch == '\n') {
      atLineStart_ = true;
      lineWidth_ = 0;
    }

    if (wrapWidth_ > 0 && lineWidth_ == wrapWidth_) {
      basicBuf_->sputc('\n');
      std::string indent(indentSize_, ' ');
      basicBuf_->sputn(indent.data(), indent.size());
      lineWidth_ = indentSize_;
      atLineStart_ = false;
    }

    lineWidth_++;
    return basicBuf_->sputc(ch);
  }

HelpPrinter& HelpPrinter::PrintUsage(const std::string& usage) {
  sbuf_.IndentSize(0);
  sbuf_.WrapWidth(0);
  Stream() << usage;
  if (usage.length() < USAGE_WIDTH) {
    Stream() <<  std::string(USAGE_WIDTH - usage.length(), ' ');
  }
  Stream() << std::string(PADDING_WIDTH, ' ');
  return *this;
}

HelpPrinter& HelpPrinter::PrintDescription(const std::string& description) {
  sbuf_.WrapWidth(USAGE_WIDTH + PADDING_WIDTH + DESCRIPTION_WIDTH);
  sbuf_.IndentSize(USAGE_WIDTH + PADDING_WIDTH);
  Stream() << description << std::endl;
  sbuf_.IndentSize(0);
  sbuf_.WrapWidth(0);
  return *this;
}

//===----------------------------------------------------------------------===//
// ChoiceOptioin.                                                             //
//===----------------------------------------------------------------------===//
ChoiceOption::ChoiceOption(const std::string& name,
                           const std::vector<std::string>& choices,
                           const std::string& help,
                           std::ostream& error)
  : OptionBase(name, help, error) {
    for (const auto& choice: choices) {
      choices_.insert(choice);
    }
  }

bool ChoiceOption::ProcessTokens(std::list<std::string> &tokens) {
  assert(0 == name_.compare(tokens.front()) && "option name is mismatched");
  if (2 != tokens.size()) {
    error() << "error: invalid option: \'" << name_ << '\'' << std::endl;
    return false;
  }

  tokens.pop_front();

  if (0 == choices_.count(tokens.front())) {
    error() << "error: invalid option: \'" << name_ << '\'' << std::endl;
    return false;
  }

  is_set_ = true;
  value_ = tokens.front();
  tokens.pop_front();
  return true;
}

void ChoiceOption::PrintHelp(HelpPrinter& printer) const {
  std::string usage = "-" + name_ + "=[";
  bool first = true;
  for (const auto& choice: choices_) {
    if (!first) {
      usage += '|';
    } else {
      first = false;
    }
    usage += choice;
  }
  usage += "]";
  printer.PrintUsage(usage).PrintDescription(help_);
}

//===----------------------------------------------------------------------===//
// OptionParser.                                                              //
//===----------------------------------------------------------------------===//
std::vector<OptionBase*>::iterator
OptionParser::FindOption(const std::string& name) {
  std::vector<OptionBase*>::iterator it = options_.begin();
  std::vector<OptionBase*>::iterator end = options_.end();
  for (; it != end; ++it) {
    if ((*it)->name() == name) {
      return it;
    }
  }
  return end;
}

bool OptionParser::AddOption(OptionBase *option) {
  if (NULL == option || !option->IsValid()) {
    return false;
  }
  if (FindOption(option->name()) != options_.end()) {
    return false;
  }
  options_.push_back(option);
  return true;
}

const std::string& OptionParser::Unknown() const {
  assert(collectUnknown_);
  return unknownOptions_;
}

bool OptionParser::ParseOptions(const char *options) {
  std::list<std::string> tokens_l1 = StringFactory::Tokenize(options, ' ');
  if (0 == tokens_l1.size()) {
    return true;
  }

  std::list<std::string>::iterator tokens_l1i = tokens_l1.begin();
  while (tokens_l1i != tokens_l1.end()) {
    if ('-' == tokens_l1i->at(0)) {
      std::list<std::string>::iterator option_begin = tokens_l1i;
      std::list<std::string> tokens_l2;
      do {
        tokens_l2.push_back(*tokens_l1i);
        tokens_l1i++;
      } while (tokens_l1i != tokens_l1.end() && '-' != tokens_l1i->at(0));
      std::list<std::string>::iterator option_end = tokens_l1i;
      tokens_l2.front().erase(0, 1);

      if (1 == tokens_l2.size()) {
        tokens_l2 = StringFactory::Tokenize(tokens_l2.front().c_str(), '=');
        if (2 < tokens_l2.size()) {
          if (collectUnknown_) {
            unknownOptions_ += *tokens_l1i + " ";
            continue;
          } else {
            error() << "error: invalid option format: \'"
                    << tokens_l2.front() << '\'' << std::endl;
            Reset();
            return false;
          }
        }
      }

      auto find_status = FindOption(tokens_l2.front());
      if (find_status == options_.end()) {
        if (collectUnknown_) {
          for (; option_begin != option_end; ++option_begin) {
            unknownOptions_ += *option_begin + " ";
          }
          continue;
        } else {
          error() << "error: unknown option: \'"
                  << tokens_l2.front() << '\'' << std::endl;
          Reset();
          return false;
        }
      }

      if (!(*find_status)->ProcessTokens(tokens_l2)) {
        Reset();
        return false;
      }
      assert(0 == tokens_l2.size());
    } else {
      if (collectUnknown_) {
        unknownOptions_ += *tokens_l1i + " ";
      } else {
        error() << "error: unknown option: \'"
                << *tokens_l1i << '\'' << std::endl;
        Reset();
        return false;
      }
    }
  }

  return true;
}

void OptionParser::PrintHelp(std::ostream& out, const std::string& addition) const {
  HelpPrinter printer(out);
  for (const auto& option: options_) {
    option->PrintHelp(printer);
  }
  out << addition << std::endl;
}

void OptionParser::Reset() {
  for (auto &option : options_) {
    option->Reset();
  }
}

} // namespace common
} // namespace ext
