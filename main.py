import os
import json
from extractor.processor import extract_title, extract_headings

def main():
    input_dir = "/app/input"
    output_dir = "/app/output"
    os.makedirs(output_dir, exist_ok=True)

    print("📄 Running PDF heading extractor...")

    for filename in os.listdir(input_dir):
        if filename.lower().endswith(".pdf"):
            pdf_path = os.path.join(input_dir, filename)
            output_path = os.path.join(output_dir, filename.rsplit(".", 1)[0] + ".json")

            title = extract_title(pdf_path)
            outline = extract_headings(pdf_path)

            with open(output_path, "w", encoding="utf-8") as f:
                json.dump({
                    "title": title,
                    "outline": outline
                }, f, indent=2, ensure_ascii=False)

    print("✅ Done.")

if __name__ == "__main__":
    main()
