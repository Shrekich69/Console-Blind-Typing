import requests
from bs4 import BeautifulSoup
from pathlib import Path

def save_quotes(quotesToWrite: str, quotesInTotal: int, path: str) -> None:
    with open(path, "w") as f:
        f.write(f"{quotesInTotal}\n")
        f.write(quotesToWrite)

file_path = Path(Path(__file__).parent.__str__() + "/quotes.txt").resolve()
if not file_path.exists():
    quotesInTotal = 0
    quotes: str = ""
    urls: list[str] = ["https://quotes.toscrape.com/", "https://quotes.toscrape.com/page/2/"]
    for url in urls:
        r = requests.get(url)

        soup = BeautifulSoup(r.content, 'html.parser')

        quotes_html = soup.select('.quote .text')
        for quote_html in quotes_html:
            quotesInTotal += 1
            # Parsing quote
            quote_text: str = quote_html.text[1:-1]
            quotes += quote_text + '\n'

    save_quotes(quotes, quotesInTotal, file_path.__str__())
