from pulp_model import select_minimize_fee

def process_call(args):

    return select_minimize_fee(args["amount"], args["wallet"])

