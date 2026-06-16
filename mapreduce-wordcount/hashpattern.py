import re
from collections import Counter

String = "Hello my name is Hello"

def hash_pattern(s: str) -> dict[str, int]:
    words = re.findall(r"\b\w+\b", s)
    return dict(Counter(words))

print(hash_pattern(String))