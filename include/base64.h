#include <array>
#include <cstdint>
#include <string_view>

namespace base64 {

    constexpr std::array<char, 64> encode_table = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
        'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
        'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
        't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', '+', '/' };

    template<typename T, typename U>
        requires std::integral<T>
    constexpr T extract_bits(T val, U start_pos, U num_bits) noexcept {
        return (val >> start_pos) & ((static_cast<T>(1) << num_bits) - 1);
    }

    template <std::size_t N>
    constexpr auto encode(const char(&str)[N]) {
   
        //Taken from: https://stackoverflow.com/questions/2745074/fast-ceiling-of-an-integer-division-in-c-c
        constexpr std::size_t output_len = 4 * ((N - 1) / 3 + ((N - 1) % 3 != 0)) + 1;
   
        std::array<char, output_len> out{};
   
        uint32_t octets = 0;
        uint32_t output_index = 0;
   
        for (int i = 0; i < N - 1; i++) {
            if (i % 3 == 0) {
                octets |= (str[i] << 16);
            }
   
            else if (i % 3 == 1) {
                octets |= (str[i] << 8);
            }
   
            else if (i % 3 == 2) {
                octets |= str[i];
                char seg_1 = extract_bits(octets, 24 - 6, 6);
                char seg_2 = extract_bits(octets, 24 - 12, 6);
                char seg_3 = extract_bits(octets, 24 - 18, 6);
                char seg_4 = extract_bits(octets, 0, 6);
   
                out[output_index] = encode_table[seg_1];
                out[output_index + 1] = encode_table[seg_2];
                out[output_index + 2] = encode_table[seg_3];
                out[output_index + 3] = encode_table[seg_4];
   
                output_index += 4;
   
                octets = 0;
            }
        }
   
        int remaining = (N - 1) % 3;
   
        if (remaining == 1) {
            char seg_1 = extract_bits(octets, 24 - 6, 6);
            char seg_2 = extract_bits(octets, 16, 2) << 4;
            out[output_index] = encode_table[seg_1];
            out[output_index + 1] = encode_table[seg_2];
            out[output_index + 2] = '=';
            out[output_index + 3] = '=';
        }
   
        else if (remaining == 2) {
            char seg_1 = extract_bits(octets, 24 - 6, 6);
            char seg_2 = extract_bits(octets, 24 - 12, 6);
            char seg_3 = extract_bits(octets, 24 - 18, 6);
            out[output_index] = encode_table[seg_1];
            out[output_index + 1] = encode_table[seg_2];
            out[output_index + 2] = encode_table[seg_3];
            out[output_index + 3] = '=';
        }
   
        out[output_len - 1] = '\0';
   
        return out;
    }

}