from dataclasses import dataclass
from dataclass_wizard import JSONWizard
from tqdm import tqdm
import json
from pathlib import Path

PATH = Path("../coin-selection-c-master/simulation/results")

@dataclass
class Coin:
    value: int

@dataclass
class DataPoint(JSONWizard):
    time: int
    coins: list[Coin]

    def __repr__(self):
        return f"[{self.time}]\t{sum(c.value for c in self.coins)}$"


def load_json_log(fname):
    with open(fname, 'r') as json_file:
        return list(DataPoint.from_dict(d) for d in tqdm(json.load(json_file)))    

def main():
    files = (f for f in Path.iterdir(PATH) if f.name.endswith(".json"))

    return {f: load_json_log(f) for f in files}


k, v = next(main().items().__iter__())
print(v[:10])

