import fitz
import re
from .utils import clean_text, merge_adjacent_spans, is_heading_like

def extract_title(pdf_path):
    doc = fitz.open(pdf_path)
    first_page = doc[0]
    candidates = []

    for block in first_page.get_text("dict")["blocks"]:
        for line in block.get("lines", []):
            for span in line.get("spans", []):
                text = clean_text(span["text"])
                if not text:
                    continue
                size = round(span["size"], 1)
                candidates.append((text, size))

    if not candidates:
        return "Untitled Document"

    # Pick top 3 largest size candidates, longest uppercase one
    top_sizes = sorted(list(set([s for _, s in candidates])), reverse=True)[:3]
    title_candidates = [t for t, s in candidates if s in top_sizes]

    # Prefer all-uppercase or proper nouns
    for t in title_candidates:
        if re.match(r'^[A-Z\s:()\-/&]{5,}$', t) or re.match(r'^([A-Z][a-z]+[\s:()\-/&]*){2,}$', t):
            return t

    return title_candidates[0]

def extract_headings(pdf_path):
    doc = fitz.open(pdf_path)
    font_counter = {}

    # Pass 1: Collect font size distribution
    for page in doc:
        for block in page.get_text("dict")["blocks"]:
            for line in block.get("lines", []):
                for span in line.get("spans", []):
                    size = round(span["size"], 1)
                    font_counter[size] = font_counter.get(size, 0) + 1

    sorted_fonts = sorted(font_counter.items(), key=lambda x: (-x[0], -x[1]))
    if not sorted_fonts:
        return []

    size_to_level = {}
    for idx, (size, _) in enumerate(sorted_fonts[:3]):
        size_to_level[size] = f"H{idx+1}"

    headings = []

    # Pass 2: Identify headings
    for page_num, page in enumerate(doc):
        spans = []
        for block in page.get_text("dict")["blocks"]:
            for line in block.get("lines", []):
                for span in line.get("spans", []):
                    text = clean_text(span["text"])
                    if not text:
                        continue
                    spans.append({
                        "text": text,
                        "size": round(span["size"], 1),
                        "font": span["font"]
                    })

        for item in merge_adjacent_spans(spans):
            text = item["text"]
            size = item["size"]
            if size in size_to_level and is_heading_like(text):
                headings.append({
                    "text": text,
                    "level": size_to_level[size],
                    "page": page_num
                })

    return headings
