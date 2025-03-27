from pulp import LpStatus
from pulp.constants import LpStatusOptimal, LpStatusInfeasible

from pulp_model import select_minimize_deposit_refresh, select_minimize_fee

def process_call_refresh(args):
    status, result = select_minimize_fee(args["amount"], args["wallet"])
    
    if status not in {LpStatusOptimal, LpStatusInfeasible}:
        message = f"""Encountered unexpected result status {LpStatus[status]}
        Requested Amount: {args["amount"]}
        Wallet: {args["wallet"]}
        """
        print(message)
        raise ValueError(message)

    return result

def process_call(args):
    status, result = select_minimize_deposit_refresh(args["amount"], args["wallet"])
    
    if status not in {LpStatusOptimal, LpStatusInfeasible}:
        message = f"""Encountered unexpected result status {LpStatus[status]}
        Requested Amount: {args["amount"]}
        Wallet: {args["wallet"]}
        """
        print(message)
        raise ValueError(message)

    return result
