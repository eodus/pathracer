#include <gtest/gtest.h>
#include "graph.hpp"
#include "aa_cursor.hpp"

// #include <iostream>

template <class GraphCursor>
std::string traverse(GraphCursor cursor) {
  std::string result;
  while (!cursor.is_empty()) {
    result += cursor.letter();
    auto nexts = cursor.next();
    if (nexts.size() == 0) {
      break;
    }
    cursor = nexts[0];
  }

  return result;
}

std::string translate_via_cursor(const std::string &nts) {
  auto nt_graph = Graph({nts});
  auto aa_cursor = make_aa_cursors(nt_graph.begins())[0];
  return traverse(aa_cursor);
}

TEST(TranslateAntibody, TRANSLATION) {
  std::string nts = "GAGGTGCAGCTGGTGGAGTCTGGGGGAGGTGTGGTACGGCCTGGGGGGTCCCTGAGACTCTCCTGTGCAGCCTCTGGATTCACCTTTGATGATTATGGCATGAGCTGGGTCCGCCAAGCTCCAGGGAAGGGGCTGGAGTGGGTCTCTGGTATTAATTGGAATGGTGGTAGCACAGGTTATGCAGACTCTGTGAAGGGCCGATTCACCATCTCCAGAGACAACGCCAAGAACTCCCTGTATCTGCAAATGAACAGTCTGAGAGCCGAGGACACGGCCTTGTATCACTGTGCGAGAGATCATGATAGTAGTAGCCCGGGGTCCAACTGGTTCGACCCCTGGGGCCAGGGAACCCTGGTCACC";
  std::string aas = "EVQLVESGGGVVRPGGSLRLSCAASGFTFDDYGMSWVRQAPGKGLEWVSGINWNGGSTGYADSVKGRFTISRDNAKNSLYLQMNSLRAEDTALYHCARDHDSSSPGSNWFDPWGQGTLVT";
  EXPECT_EQ(translate(nts), aas);
}

TEST(TranslateViaCursorAntibody, TRANSLATION) {
  std::string nts = "GAGGTGCAGCTGGTGGAGTCTGGGGGAGGTGTGGTACGGCCTGGGGGGTCCCTGAGACTCTCCTGTGCAGCCTCTGGATTCACCTTTGATGATTATGGCATGAGCTGGGTCCGCCAAGCTCCAGGGAAGGGGCTGGAGTGGGTCTCTGGTATTAATTGGAATGGTGGTAGCACAGGTTATGCAGACTCTGTGAAGGGCCGATTCACCATCTCCAGAGACAACGCCAAGAACTCCCTGTATCTGCAAATGAACAGTCTGAGAGCCGAGGACACGGCCTTGTATCACTGTGCGAGAGATCATGATAGTAGTAGCCCGGGGTCCAACTGGTTCGACCCCTGGGGCCAGGGAACCCTGGTCACC";
  EXPECT_EQ(translate_via_cursor(nts), translate(nts));
}

TEST(StopCodons, TRANSLATION) {
  std::string nts = "TAGTAATGA";
  std::string aas = "XXX";
  EXPECT_EQ(translate(nts), aas);
}

// int main() {
//   std::string s = "GAGGTGCAGCTGGTGGAGTCTGGGGGAGGTGTGGTACGGCCTGGGGGGTCCCTGAGACTCTCCTGTGCAGCCTCTGGATTCACCTTTGATGATTATGGCATGAGCTGGGTCCGCCAAGCTCCAGGGAAGGGGCTGGAGTGGGTCTCTGGTATTAATTGGAATGGTGGTAGCACAGGTTATGCAGACTCTGTGAAGGGCCGATTCACCATCTCCAGAGACAACGCCAAGAACTCCCTGTATCTGCAAATGAACAGTCTGAGAGCCGAGGACACGGCCTTGTATCACTGTGCGAGAGATCATGATAGTAGTAGCCCGGGGTCCAACTGGTTCGACCCCTGGGGCCAGGGAACCCTGGTCACC";
//   std::string translated = "EVQLVESGGGVVRPGGSLRLSCAASGFTFDDYGMSWVRQAPGKGLEWVSGINWNGGSTGYADSVKGRFTISRDNAKNSLYLQMNSLRAEDTALYHCARDHDSSSPGSNWFDPWGQGTLVT";
//   auto graph = Graph({s});
//
//   auto aa_cursor = make_aa_cursors(graph.begins())[0];
//
//   std::cout << traverse(aa_cursor) << std::endl;
//
//   return 0;
// }
