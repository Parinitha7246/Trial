import re

def clean_text(text):
    return re.sub(r'\s+', ' ', text.strip())

def merge_adjacent_spans(spans):
    merged = []
    skip = False
    for i in range(len(spans)):
        if skip:
            skip = False
            continue
        current = spans[i]
        if i < len(spans) - 1:
            next_span = spans[i + 1]
            if current["size"] == next_span["size"] and current["font"] == next_span["font"]:
                merged.append({
                    "text": f"{current['text']} {next_span['text']}",
                    "size": current["size"],
                    "font": current["font"]
                })
                skip = True
            else:
                merged.append(current)
        else:
            merged.append(current)
    return merged

def is_heading_like(text):
    text = text.strip()

    if not text or len(text) > 150:
        return False
    if len(text.split()) > 15:
        return False
    if re.search(r'\.(com|org|edu|http)', text, re.IGNORECASE):
        return False
    if re.match(r'^[a-z]', text):  # lowercase start = likely paragraph
        return False
    if re.search(r'\w+\.\w{2,4}$', text):  # skip links/emails
        return False

    # Allow titles like "1. Objective", "I. Introduction"
    if re.match(r'^(\(?[ivxIVX\d]{1,4}\)?\.?)\s+\w+', text):
        return True
    if re.match(r'^[A-Z][\w\s:()\-/,&]+$', text) and not text.endswith('.'):
        return True
    return False
